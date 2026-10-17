/* A password-protected profile that was selected when ProsperoTube closed is not entered
 * again without its password: browsing starts as Guest, the startup folder asks for the
 * password, and the saved choice stays until a profile is chosen. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char parent[80];
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static const char *saved_profile(void) {
    static char value[40];
    cJSON *store=read_store();snprintf(value,sizeof value,"%s",cJSON_GetObjectItem(store,"library_profile")->valuestring);
    cJSON_Delete(store);return value;
}
static int locks(void) {cJSON *store=read_store();int n=cJSON_GetArraySize(cJSON_GetObjectItem(store,"profile_locks"));cJSON_Delete(store);return n;}
/* The profile browsing happens under, as the library menu names it. */
static const char *active(void) {
    static char value[64];
    show("library");assert(!strncmp(last_items[0].title,"Profile: ",9));snprintf(value,sizeof value,"%.60s",last_items[0].title+9);return value;
}
/* The codec in use stands for a profile's own settings (0 automatic, the default; 1 AVC ... 4 HEVC).
 * Until 38d2840 the maximum quality did; since then it is 2160p for every profile. */
static int codec(void) {int value=-1,height=0;tube_player_quality(&height,NULL,&value);assert(height==2160);return value;}
static void set_codec(int wanted) {for(int i=0;i<5 && codec()!=wanted;++i)show("settings:codec");assert(codec()==wanted);}
static int favorites(void) {p->list_catalog("favorites",0,items_cb,NULL);assert(last_ok);return last_count;}
/* Take the pending prompt and answer it; -2 when no prompt is pending. */
static int answer(const char *text) {
    tube_playlist_prompt_t prompt;
    if(!tube_take_playlist_prompt(&prompt))return -2;
    assert(!strcmp(prompt.label,"Password of this profile") && !*prompt.initial);
    return tube_submit_playlist_name(prompt.ticket,text,parent,sizeof parent);
}
static void restart(void) {p->shutdown();assert(!p->init());}
static int waiting(void) {return !strcmp(tube_boot_section(),"library:profile:alice") && !strcmp(tube_boot_section_title(),"Accounts & library");}
int main(void) {
    char dir[]="/tmp/tube-startup-lock-XXXXXX";tube_playlist_prompt_t prompt;
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    /* alice: her own codec, startup section, a favorite and a password. */
    show("library:profile:alice");set_codec(2);show("settings:boot-section:2");assert(tube_toggle_favorite("abcdefghijk")==1);
    assert(!strcmp(tube_boot_section(),"library:subscriptions") && !strcmp(tube_boot_section_title(),"Subscriptions"));
    show("settings:lock:set");assert(tube_take_playlist_prompt(&prompt) && !tube_submit_playlist_name(prompt.ticket,"secret",parent,sizeof parent) && locks()==1);
    assert(!strcmp(active(),"alice") && !waiting()); /* selected with the password known: no question in this session */
    /* A profile without a password starts as before. */
    show("library:profile:bob");restart();assert(!strcmp(active(),"bob") && !*tube_boot_section() && !strcmp(saved_profile(),"bob"));
    show("library:profile:alice");assert(answer("secret")==0);show(parent);assert(!strcmp(active(),"alice"));
    /* Restart: Guest is active, nothing of alice is listed, and the startup folder is her password prompt. */
    restart();
    assert(waiting() && !strcmp(active(),"guest") && codec()==0 && !favorites());
    show("settings:captions");assert(!strcmp(saved_profile(),"alice")); /* a save keeps the choice that waits */
    show(tube_boot_section());assert(last_count==1 && !strcmp(last_items[0].title,"Enter the password of this profile") && !strcmp(active(),"guest"));
    show(tube_boot_section());assert(answer("wrong")==-1 && !strcmp(tube_prompt_error(),"Wrong password.") && waiting() && !strcmp(active(),"guest"));
    /* Closing without the password asks again next time. */
    restart();assert(waiting() && !strcmp(active(),"guest") && !favorites() && !strcmp(saved_profile(),"alice"));
    show(tube_boot_section());assert(answer("secret")==0 && !strcmp(parent,"library:profile:alice") && waiting());
    show(parent);assert(last_count>3 && strstr(last_items[0].title,"alice") && !waiting());
    assert(!strcmp(active(),"alice") && codec()==2 && favorites()==1 && !strcmp(tube_boot_section(),"library:subscriptions"));
    /* Every start asks while she stays selected; choosing another profile ends the wait and becomes the saved choice. */
    restart();assert(waiting() && !strcmp(active(),"guest"));
    show("library:profile:guest");assert(!waiting() && !*tube_boot_section() && !strcmp(saved_profile(),"guest"));
    restart();assert(!waiting() && !strcmp(active(),"guest"));
    show("library:profile:alice");assert(last_count==1 && answer("secret")==0);show(parent);assert(!strcmp(active(),"alice"));
    restart();assert(waiting());show("library:profile:bob");assert(!waiting() && !strcmp(active(),"bob") && !strcmp(saved_profile(),"bob"));
    /* With one settings set for every profile the question is the same. */
    show("settings:separate-settings");show("library:profile:alice");assert(answer("secret")==0);show(parent);assert(!strcmp(active(),"alice"));
    restart();assert(waiting() && !strcmp(active(),"guest") && !favorites() && !strcmp(saved_profile(),"alice"));
    show("settings:separate-settings");
    /* The account list at startup also leads to the prompt. */
    show("settings:account-on-boot");assert(tube_select_account_on_boot() && waiting());
    show("library:profile:alice");assert(last_count==1 && answer("secret")==0);show(parent);assert(!strcmp(active(),"alice") && !waiting());
    show("settings:account-on-boot");
    /* A waiting profile that left the companion loses its password; Guest becomes the saved choice. */
    restart();assert(waiting() && locks()==1);
    assert(!p->set_source("http://127.0.0.1:8765#123456") && waiting() && !strcmp(saved_profile(),"alice"));
    p->list_catalog("library:profiles",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"profiles\":[{\"id\":\"guest\",\"name\":\"Guest\"},{\"id\":\"bob\",\"name\":\"Bob\",\"account_configured\":true}]}}");
    assert(last_ok && !waiting() && !locks() && !strcmp(saved_profile(),"guest") && !strcmp(active(),"guest"));
    restart();assert(!waiting() && !strcmp(active(),"guest"));
    /* While the profile is still listed the wait continues. */
    show("library:profile:alice");show("settings:lock:set");assert(tube_take_playlist_prompt(&prompt) && !tube_submit_playlist_name(prompt.ticket,"again",parent,sizeof parent));
    restart();assert(waiting());
    p->list_catalog("library:profiles",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"profiles\":[{\"id\":\"guest\",\"name\":\"Guest\"},{\"id\":\"alice\",\"name\":\"Alice\",\"account_configured\":true}]}}");
    assert(last_ok && waiting() && locks()==1 && !strcmp(saved_profile(),"alice"));
    show("library:profile:alice");assert(last_count==1 && answer("secret")==-1 && answer("again")==-2);
    show("library:profile:alice");assert(answer("again")==0);show(parent);assert(!strcmp(active(),"alice") && !waiting());
    p->shutdown();
    puts("PASS: startup challenge of a protected selected profile, Guest until the password is given, saved choice kept, other profiles and removal");
}
