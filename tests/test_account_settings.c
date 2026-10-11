/* Account settings: one shared settings set or one per account, and an optional
 * password asked before a profile is selected. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_password.h"
#include "tube_player_extra.h"
static const evo_provider_t *p;
static char parent[80];
static unsigned ticket;
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void refused(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count);}
static int find(const char *id) {for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,id))return i;return -1;}
static cJSON *profile_row(cJSON *store,const char *profile) {
    cJSON *row;
    cJSON_ArrayForEach(row,cJSON_GetObjectItem(store,"profile_settings"))
        if(!strcmp(cJSON_GetObjectItem(row,"profile")->valuestring,profile))return row;
    return NULL;
}
/* Saved maximum quality of a settings row; -1 when the row does not exist. */
static int height_of(const char *profile) {
    cJSON *store=read_store(),*row=profile_row(store,profile);
    int value=row?cJSON_GetObjectItem(row,"max_height")->valueint:-1;
    cJSON_Delete(store);return value;
}
static int height(void) {
    for(int page=0;page<3;++page) {
        p->list_catalog("settings",page,items_cb,NULL);assert(last_ok);
        int row=find("settings:quality"),value=0;
        if(row>=0){assert(sscanf(last_items[row].title,"Maximum quality: %dp",&value)==1);return value;}
    }
    return -1;
}
static const char *selected_profile(void) {
    static char value[40];
    cJSON *store=read_store();snprintf(value,sizeof value,"%s",cJSON_GetObjectItem(store,"library_profile")->valuestring);
    cJSON_Delete(store);return value;
}
static int separate(void) {
    cJSON *store=read_store(),*item=cJSON_GetObjectItem(store,"separate_settings");
    int value=cJSON_IsBool(item)?cJSON_IsTrue(item):-1;cJSON_Delete(store);return value;
}
static int locks(const char *profile) {
    cJSON *store=read_store(),*row;int count=0;
    cJSON_ArrayForEach(row,cJSON_GetObjectItem(store,"profile_locks"))
        if(!profile || !strcmp(cJSON_GetObjectItem(row,"profile")->valuestring,profile)) {
            assert(tube_password_record_valid(cJSON_GetObjectItem(row,"password")->valuestring));++count;
        }
    cJSON_Delete(store);return count;
}
static int file_contains(const char *text) {
    FILE *f=fopen(evo_data_path("youtube.json"),"rb");assert(f);
    static char body[262144];size_t n=fread(body,1,sizeof body-1,f);body[n]=0;fclose(f);
    return strstr(body,text)!=NULL;
}
/* Take the pending prompt and answer it; -2 when no prompt is pending. */
static int answer(const char *label,const char *text) {
    tube_playlist_prompt_t prompt;
    if(!tube_take_playlist_prompt(&prompt))return -2;
    assert(!strcmp(prompt.label,label) && !*prompt.initial);ticket=prompt.ticket;
    return tube_submit_playlist_name(prompt.ticket,text,parent,sizeof parent);
}
static void hex(const unsigned char *hash,char *out) {for(int i=0;i<32;++i)snprintf(out+i*2,3,"%02x",hash[i]);}
int main(void) {
    char dir[]="/tmp/tube-account-settings-XXXXXX",text[65];unsigned char hash[32],salt[8]={1,2,3,4,5,6,7,8},other[8]={8,7,6,5,4,3,2,1};
    /* Password records: SHA-256 as specified, salted, iterated, compared in full. */
    tube_sha256((const unsigned char *)"abc",3,hash);hex(hash,text);assert(!strcmp(text,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    tube_sha256((const unsigned char *)"",0,hash);hex(hash,text);assert(!strcmp(text,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    tube_sha256((const unsigned char *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",56,hash);hex(hash,text);
    assert(!strcmp(text,"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    char record[TUBE_PASSWORD_RECORD+1],again[TUBE_PASSWORD_RECORD+1],salted[TUBE_PASSWORD_RECORD+1];
    assert(tube_password_make(salt,"secret",record) && tube_password_make(salt,"secret",again) && tube_password_make(other,"secret",salted));
    assert(strlen(record)==TUBE_PASSWORD_RECORD && !strcmp(record,again) && strcmp(record,salted) && !strncmp(record,"v1$0102030405060708$",20) && !strstr(record,"secret"));
    assert(tube_password_record_valid(record) && tube_password_check(record,"secret") && tube_password_check(salted,"secret"));
    assert(!tube_password_check(record,"Secret") && !tube_password_check(record,"secret ") && !tube_password_check(record,"") && !tube_password_check(record,NULL));
    assert(!tube_password_make(salt,"",again) && !tube_password_make(NULL,"x",again) && !tube_password_check("secret","secret"));
    snprintf(again,sizeof again,"%s",record);again[30]='G';assert(!tube_password_record_valid(again) && !tube_password_check(again,"secret"));
    snprintf(again,sizeof again,"%s",record);again[1]='2';assert(!tube_password_record_valid(again));
    assert(!tube_password_record_valid("v1$0102030405060708$") && !tube_password_record_valid(NULL));
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    /* A new installation shares one settings set between all profiles. */
    show("settings:accounts");assert(last_count==2 && !strcmp(last_items[0].id,"settings:separate-settings") && !strcmp(last_items[0].title,"Separate settings per account: off"));
    assert(!strcmp(last_items[1].title,"Password protection (experimental)") && find("settings:lock:set")<0); /* Guest has no account to protect */
    show("settings:quality:720");assert(tube_player_set_hide_seconds(7) && tube_extra_set_volume(150) && tube_set_playback_speed(125));
    show("settings:hide:mixes");show("settings:content-language:de");
    assert(separate()==0 && height_of("*")==720 && height_of("guest")==-1);
    show("library:profile:alice");
    assert(height()==720 && tube_player_hide_ms()==7000 && tube_extra_volume()==150 && tube_playback_speed()==125 && !strcmp(selected_profile(),"alice"));
    show("settings:hide");assert(!strcmp(last_items[1].title,"Hide mixes: on"));
    show("settings:quality:480");show("library:profile:guest");assert(height()==480 && height_of("alice")==-1);
    p->shutdown();assert(!p->init());
    assert(height()==480 && tube_player_hide_ms()==7000 && tube_extra_volume()==150 && separate()==0);
    show("library:profile:alice");assert(height()==480 && tube_extra_volume()==150);
    /* Switched on, the selected profile keeps what is on screen and the others use their own sets. */
    show("settings:separate-settings");assert(!strcmp(last_items[0].title,"Separate settings per account: on") && separate()==1);
    assert(height()==480 && tube_player_hide_ms()==7000 && tube_extra_volume()==150 && height_of("alice")==480);
    show("settings:quality:1440");assert(tube_player_set_hide_seconds(2) && tube_extra_set_volume(50));
    show("library:profile:guest");assert(height()==1080 && tube_player_hide_ms()==3000 && tube_extra_volume()==100 && tube_playback_speed()==100);
    show("settings:hide");assert(!strcmp(last_items[1].title,"Hide mixes: off"));
    show("settings:quality:360");
    show("library:profile:alice");assert(height()==1440 && tube_player_hide_ms()==2000 && tube_extra_volume()==50 && tube_playback_speed()==125);
    p->shutdown();assert(!p->init());assert(height()==1440 && separate()==1);
    show("library:profile:guest");assert(height()==360);
    /* Switched off again, everyone uses what the selected profile had on screen. */
    show("library:profile:alice");show("settings:separate-settings");assert(separate()==0 && height()==1440 && tube_player_hide_ms()==2000 && tube_extra_volume()==50);
    show("library:profile:guest");assert(height()==1440 && tube_player_hide_ms()==2000 && tube_extra_volume()==50);
    show("settings:quality:2160");assert(height_of("*")==2160 && height_of("guest")==360 && height_of("alice")==1440); /* the own sets are kept */
    show("library:profile:bob");assert(height()==2160);
    show("library:profile:guest");show("settings:separate-settings");assert(separate()==1 && height()==2160 && height_of("guest")==2160);
    show("library:profile:alice");assert(height()==1440 && tube_player_hide_ms()==2000);
    show("library:profile:bob");assert(height()==1080); /* a profile without a set of its own starts at the defaults */
    /* A source change returns to Guest in either mode. */
    assert(!p->set_source("http://127.0.0.1:8765"));assert(height()==2160 && !strcmp(selected_profile(),"guest"));
    /* A store written before the switch existed keeps one set per profile; a corrupt switch reads the same way. */
    p->shutdown();cJSON *store=read_store();cJSON_DeleteItemFromObject(store,"separate_settings");write_store(store);cJSON_Delete(store);
    assert(!p->init());assert(height()==2160);
    show("settings:accounts");assert(!strcmp(last_items[0].title,"Separate settings per account: on"));
    show("library:profile:alice");assert(height()==1440);
    p->shutdown();store=read_store();cJSON_ReplaceItemInObject(store,"separate_settings",cJSON_CreateString("off"));write_store(store);cJSON_Delete(store);
    assert(!p->init());assert(height()==1440);
    show("settings:accounts");assert(!strcmp(last_items[0].title,"Separate settings per account: on"));
    show("library:profile:guest");assert(height()==2160 && separate()==1);
    /* A store that names the switch off but has no shared row starts the shared set at the defaults. */
    p->shutdown();store=read_store();cJSON_ReplaceItemInObject(store,"separate_settings",cJSON_CreateFalse());
    cJSON *rows=cJSON_GetObjectItem(store,"profile_settings");
    for(int i=cJSON_GetArraySize(rows)-1;i>=0;--i)
        if(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(rows,i),"profile")->valuestring,"*"))cJSON_DeleteItemFromArray(rows,i);
    write_store(store);cJSON_Delete(store);assert(!p->init());assert(height()==1080);
    show("library:profile:alice");assert(height()==1080);
    show("settings:separate-settings");assert(separate()==1);show("library:profile:guest");assert(height()==2160);
    /* Protect a profile: the password is asked for twice in the file's lifetime, and never stored. */
    refused("settings:lock:set");refused("settings:lock:change");refused("settings:lock:remove"); /* Guest */
    show("library:profile:alice");
    show("settings:accounts");assert(last_count==2 && !strcmp(last_items[1].id,"settings:lock:set"));
    assert(!strcmp(last_items[1].title,"Protect this profile with a password (experimental)") && strstr(last_items[1].overview,"Experimental: tested offline only"));
    refused("settings:lock:change");refused("settings:lock:remove");refused("settings:lock:new:1");refused("settings:lock:bogus");
    show("settings:lock:set");assert(last_count==1 && !strcmp(last_items[0].title,"Enter the password"));
    assert(answer("New password of this profile","  ")==-1 && !strcmp(tube_prompt_error(),"Enter a password of 1 to 150 characters.") && !locks(NULL));
    assert(tube_submit_playlist_name(ticket,"correct horse",parent,sizeof parent)==0 && !*tube_prompt_error() && !strcmp(parent,"settings:accounts"));
    assert(!strcmp(tube_prompt_title(parent),"Accounts") && locks(NULL)==1 && locks("alice")==1 && !file_contains("correct horse"));
    assert(tube_submit_playlist_name(ticket,"another",parent,sizeof parent)==-1); /* the prompt is spent */
    show(parent);assert(last_count==3 && !strcmp(last_items[1].id,"settings:lock:change") && !strcmp(last_items[2].id,"settings:lock:remove"));
    refused("settings:lock:set");
    /* Selecting the protected profile asks first; a wrong text selects nothing. */
    show("library:profile:alice");assert(last_count>3 && answer("Password of this profile","x")==-2); /* already selected: no question */
    show("library:profile:guest");assert(!strcmp(selected_profile(),"guest") && height()==2160);
    show("library:profile:alice");assert(last_count==1 && !strcmp(last_items[0].title,"Enter the password of this profile") && !strcmp(selected_profile(),"guest"));
    assert(answer("Password of this profile","correct")==-1 && !strcmp(tube_prompt_error(),"Wrong password.") && !strcmp(selected_profile(),"guest"));
    show("library:profile:alice");assert(last_count==1 && height()==2160);
    assert(answer("Password of this profile","correct horse")==0 && !strcmp(parent,"library:profile:alice") && !strcmp(tube_prompt_title(parent),"Accounts & library"));
    assert(!strcmp(selected_profile(),"guest")); /* the accepted text only permits the selection */
    show(parent);assert(last_count>3 && strstr(last_items[0].title,"alice") && !strcmp(selected_profile(),"alice") && height()==1080);
    /* The permission serves one selection and only the profile it was given for. */
    show("library:profile:guest");show("library:profile:alice");assert(last_count==1 && !strcmp(selected_profile(),"guest"));
    assert(answer("Password of this profile","correct horse")==0);
    show("library:profile:bob");assert(!strcmp(selected_profile(),"bob"));
    show("library:profile:alice");assert(last_count==1 && !strcmp(selected_profile(),"bob"));
    assert(answer("Password of this profile","correct horse")==0);
    assert(!p->set_source("http://127.0.0.1:8765"));
    show("library:profile:alice");assert(last_count==1 && !strcmp(selected_profile(),"bob")); /* setting the source again withdraws it */
    assert(answer("Password of this profile","correct horse")==0);
    p->shutdown();assert(!p->init());assert(locks("alice")==1);
    show("library:profile:alice");assert(last_count==1 && !strcmp(selected_profile(),"bob")); /* so does a restart */
    assert(answer("Password of this profile","correct horse")==0);show(parent);assert(!strcmp(selected_profile(),"alice"));
    /* Change: the current password, then the new one. */
    show("settings:lock:change");assert(answer("Current password of this profile","wrong")==-1 && !strcmp(tube_prompt_error(),"Wrong password."));
    show("settings:lock:change");assert(answer("Current password of this profile","correct horse")==0 && !strncmp(parent,"settings:lock:new:",18));
    char renew[80];snprintf(renew,sizeof renew,"%s",parent);
    show(renew);assert(last_count==1 && answer("New password of this profile","battery staple")==0 && !strcmp(parent,"settings:accounts"));
    refused(renew);assert(locks("alice")==1 && !file_contains("battery staple") && !file_contains("correct horse"));
    show("library:profile:guest");show("library:profile:alice");assert(answer("Password of this profile","correct horse")==-1);
    show("library:profile:alice");assert(answer("Password of this profile","battery staple")==0);show(parent);assert(!strcmp(selected_profile(),"alice"));
    /* Another profile's password and the shared settings mode do not interfere. */
    show("library:profile:bob");show("settings:lock:set");assert(answer("New password of this profile","bob's own")==0 && locks(NULL)==2);
    show("settings:separate-settings");assert(separate()==0);
    show("library:profile:alice");assert(last_count==1 && answer("Password of this profile","bob's own")==-1 && !strcmp(selected_profile(),"bob"));
    show("library:profile:alice");assert(answer("Password of this profile","battery staple")==0);show(parent);assert(!strcmp(selected_profile(),"alice"));
    show("settings:separate-settings");assert(separate()==1);
    /* Remove: the current password once more. */
    show("settings:lock:remove");assert(answer("Current password of this profile","nope")==-1 && locks("alice")==1);
    show("settings:lock:remove");assert(answer("Current password of this profile","battery staple")==0 && !strcmp(parent,"settings:accounts") && !locks("alice") && locks("bob")==1);
    show(parent);assert(!strcmp(last_items[1].id,"settings:lock:set"));
    show("library:profile:guest");show("library:profile:alice");assert(last_count>3 && !strcmp(selected_profile(),"alice"));
    /* Corrupt records protect nothing and are not written back; the first valid one of a profile wins. */
    p->shutdown();store=read_store();
    char first[TUBE_PASSWORD_RECORD+1],second[TUBE_PASSWORD_RECORD+1];
    assert(tube_password_make(salt,"first",first) && tube_password_make(other,"second",second));
    cJSON *list=cJSON_Parse("[{\"profile\":\"alice\",\"password\":\"plain text\"},{\"profile\":\"../x\",\"password\":\"\"},{\"profile\":\"carol\"},7]"),*row=cJSON_CreateObject();
    cJSON_AddStringToObject(row,"profile","carol");cJSON_AddStringToObject(row,"password",first);cJSON_AddItemToArray(list,row);
    row=cJSON_CreateObject();cJSON_AddStringToObject(row,"profile","carol");cJSON_AddStringToObject(row,"password",second);cJSON_AddItemToArray(list,row);
    cJSON_ReplaceItemInObject(store,"profile_locks",list);write_store(store);cJSON_Delete(store);
    assert(!p->init());show("settings:quality:720");assert(locks(NULL)==1 && locks("carol")==1 && !file_contains("plain text"));
    show("library:profile:guest");show("library:profile:alice");assert(!strcmp(selected_profile(),"alice"));
    show("library:profile:carol");assert(last_count==1 && answer("Password of this profile","second")==-1);
    show("library:profile:carol");assert(answer("Password of this profile","first")==0);show(parent);assert(!strcmp(selected_profile(),"carol"));
    /* An account the companion no longer lists loses its password with its selection. */
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    assert(!strcmp(selected_profile(),"carol") && locks("carol")==1); /* pairing again keeps the selection */
    p->list_catalog("library:profiles",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"profiles\":[{\"id\":\"guest\",\"name\":\"Guest\"},{\"id\":\"alice\",\"name\":\"Alice\",\"account_configured\":true}]}}");
    assert(last_ok && !strcmp(selected_profile(),"guest") && !locks(NULL));
    show("library:profile:carol");assert(!strcmp(selected_profile(),"carol"));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: shared and separate settings, migration of earlier stores, salted password records, protected profile selection, change and removal");
    return 0;
}
