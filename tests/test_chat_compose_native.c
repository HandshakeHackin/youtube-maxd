/* Shipping compose/review/send, deterministic network and no account mutation.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define main reader_baseline_main
#include "test_live_chat_native.c"
#undef main

static void open_chat(const evo_provider_t *p) {
    p->list_catalog("chat:top:abcdefghijk",0,received,NULL);
    assert(count==1 && (strstr(pending[0].body,"live_chat") || strstr(pending[0].body,"chat_poll")));
    respond(frame("owned-chat-cap","top",0,1)); assert(ok && !count);
}
static tube_chat_prompt_t compose(const evo_provider_t *p) {
    tube_chat_prompt_t prompt;
    p->list_catalog("message:compose",0,received,NULL);
    assert(ok && !count && tube_take_chat_prompt(&prompt));
    assert(!tube_take_chat_prompt(&prompt)); return prompt;
}
static void finish_job(const char *result) {
    assert(count==1 && strstr(pending[0].url,"/v1/jobs"));
    reply(0,200,"{\"job\":\"chat-job\"}");
    clock_ms+=501; tube_poll(); assert(count==1 && strstr(pending[0].url,"/v1/jobs/chat-job"));
    reply(0,200,result);
}
int main(void) {
    char directory[]="/tmp/prosperotube-compose-XXXXXX";
    assert(mkdtemp(directory)); assert(!setenv("EVO_DATA_DIR_OVERRIDE",directory,1));
    const evo_provider_t *p=&evo_provider_youtube; assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("chat:top:abcdefghijk",0,received,NULL);
    reply(0,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    respond(frame("guest-chat","top",0,1)); assert(ok);
    p->list_catalog("message:compose",0,received,NULL); assert(!ok && !count);
    tube_chat_prompt_t prompt; assert(!tube_take_chat_prompt(&prompt));
    p->list_catalog("library:profile:one",0,received,NULL); cleanup(); open_chat(p);
    prompt=compose(p);
    char review[80];
    const char *invalid[]={""," ","\xc2\xa0\xe3\x80\x80","line\nbreak","\xed\xa0\x80","\xf0\x80\x80\x80","\xf4\x90\x80\x80"};
    for (size_t i=0;i<sizeof invalid/sizeof *invalid;++i)
        assert(tube_submit_chat_message(prompt.ticket,invalid[i],review,sizeof review)<0);
    char large[805]; for(int i=0;i<201;++i) memcpy(large+i*4,"\xf0\x9f\x98\x80",4); large[804]=0;
    assert(tube_submit_chat_message(prompt.ticket,large,review,sizeof review)<0);
    large[800]=0;
    assert(!tube_submit_chat_message(prompt.ticket,large,review,sizeof review));
    assert(tube_submit_chat_message(prompt.ticket,"Again",review,sizeof review)<0 && !count);
    p->list_catalog(review,0,received,NULL); assert(ok && rows==1 && !count);
    assert(strstr(tube_comment_text(items[0].id),large));
    char send[160]; snprintf(send,sizeof send,"%s",items[0].id);
    p->list_catalog(send,0,received,NULL); assert(count==1 && strstr(pending[0].body,"chat_send"));
    cJSON *job=cJSON_Parse(pending[0].body), *request=cJSON_GetObjectItem(job,"library");
    assert(!strcmp(cJSON_GetObjectItem(request,"cursor")->valuestring,"owned-chat-cap"));
    assert(!strcmp(cJSON_GetObjectItem(request,"profile")->valuestring,"one"));
    assert(!strcmp(cJSON_GetObjectItem(request,"text")->valuestring,large));
    char session[65]; snprintf(session,sizeof session,"%s",cJSON_GetObjectItem(request,"session")->valuestring);
    cJSON_Delete(job);
    int before=count; p->list_catalog(send,0,received,NULL); assert(ok && count==before);
    char result[512]; snprintf(result,sizeof result,"{\"state\":\"done\",\"result\":{\"api_version\":1,\"success\":true,\"id\":\"abcdefghijk\",\"session\":\"%s\",\"message_id\":\"confirmed-message\"}}",session);
    finish_job(result); assert(ok && strstr(items[0].title,"sent") && !count);
    p->list_catalog(send,0,received,NULL); assert(ok && !count);
    assert(!*tube_comment_text(send));
    open_chat(p); prompt=compose(p); tube_cancel_chat_prompt(prompt.ticket);
    assert(tube_submit_chat_message(prompt.ticket,"Canceled",review,sizeof review)<0 && !count);
    prompt=compose(p); open_chat(p);
    assert(tube_submit_chat_message(prompt.ticket,"Late keyboard",review,sizeof review)<0);
    prompt=compose(p); assert(!tube_submit_chat_message(prompt.ticket,"Literal <b> & \" 日本語",review,sizeof review));
    p->list_catalog(review,0,received,NULL); snprintf(send,sizeof send,"%s",items[0].id);
    p->list_catalog(send,0,received,NULL);
    finish_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"success\":true,\"id\":\"different11\",\"session\":\"wrong\",\"message_id\":\"other\"}}");
    assert(!ok && !count); p->list_catalog(send,0,received,NULL); assert(!ok && !count);
    open_chat(p); prompt=compose(p);
    p->list_catalog("library:profile:two",0,received,NULL); cleanup();
    assert(tube_submit_chat_message(prompt.ticket,"Wrong profile",review,sizeof review)<0);
    open_chat(p); prompt=compose(p);
    p->set_source("http://127.0.0.1:8766#654321"); cleanup();
    assert(tube_submit_chat_message(prompt.ticket,"Wrong source",review,sizeof review)<0);
    privacy(); p->shutdown(); cleanup(); unlink(evo_data_path("youtube.json")); rmdir(directory);
    puts("PASS: shipping chat compose/review, Unicode bounds, no early send, single confirmation, cancellation and scope invalidation");
}
