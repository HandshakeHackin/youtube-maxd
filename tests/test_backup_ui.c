/* Backup and restore in YouTube preferences: a dated copy on request, the list, restore after
 * a review screen with the provider reloaded from the restored file, delete, and the
 * device-wide automatic backup made at startup. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_backup.h"
static const evo_provider_t *p;
static char row[160];
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void refused(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count);}
static int find(const char *title) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title)){snprintf(row,sizeof row,"%s",last_items[i].id);return i;}
    return -1;
}
static int backups(int *safety) {
    tube_backup_entry entries[TUBE_BACKUP_LIST_MAX];int count=tube_backup_list(evo_data_dir(),entries,TUBE_BACKUP_LIST_MAX);
    assert(count>=0);*safety=0;
    for(int i=0;i<count;++i)*safety+=entries[i].safety;
    return count;
}
static int saved_period(void) {
    cJSON *store=read_store();int value=cJSON_GetObjectItem(store,"backup_auto_days")->valueint;cJSON_Delete(store);return value;
}
static void wipe(void) {
    tube_backup_entry entries[TUBE_BACKUP_LIST_MAX];int count=tube_backup_list(evo_data_dir(),entries,TUBE_BACKUP_LIST_MAX);
    for(int i=0;i<count;++i)assert(tube_backup_delete(evo_data_dir(),entries[i].name)==TUBE_BACKUP_OK);
}
int main(void) {
    char dir[]="/tmp/tube-backup-ui-XXXXXX",entry[200],restore[160],erase[160],stale[160],name[TUBE_BACKUP_NAME_SIZE];int safety;
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    /* The folder: create, list, automatic period (off) and the subscriptions import. */
    int more=1,listed=0;
    for(int page=0;more;++page) {
        p->list_catalog("settings",page,items_cb,NULL);assert(last_ok);
        if(find("Backup and restore")>=0){listed=1;assert(!strcmp(row,"settings:backup"));}
        more=last_more;
    }
    assert(listed);
    show("settings:backup");assert(last_count==4 && !strcmp(last_items[0].id,"settings:backup:create") && !strcmp(last_items[1].id,"settings:backup:list"));
    assert(!strcmp(last_items[1].subtitle,"No backup yet") && !strcmp(last_items[2].title,"Automatic backup: off") && !strcmp(last_items[2].id,"settings:backup:auto:1"));
    assert(!strcmp(last_items[3].id,"settings:backup:import") && !backups(&safety));
    show("settings:backup:list");assert(last_count==1 && !strcmp(last_items[0].title,"No backup yet"));
    /* Create: the state is saved first, then copied under a dated name. */
    assert(tube_set_playback_speed(150));
    show("settings:backup:create");assert(last_count==5 && !strcmp(last_items[0].title,"Backup created") && tube_backup_name_valid(last_items[0].subtitle));
    snprintf(name,sizeof name,"%.39s",last_items[0].subtitle);
    assert(backups(&safety)==1 && !safety && tube_backup_validate(evo_data_dir(),name)==TUBE_BACKUP_OK);
    assert(strstr(last_items[2].subtitle,"1 kept"));
    p->list_catalog("settings:backup:create",1,items_cb,NULL);assert(last_ok && backups(&safety)==1); /* a later page creates nothing */
    show("settings:backup");assert(last_count==4); /* the notice belongs to the action that made it */
    show("settings:backup:list");assert(last_count==1 && strstr(last_items[0].title," UTC") && strstr(last_items[0].subtitle,"KiB"));
    snprintf(entry,sizeof entry,"settings:backup:entry:%s",name);assert(!strcmp(last_items[0].id,entry));
    /* The state moves on; the entry screen reviews the restore and issues a ticket for one action. */
    assert(tube_set_playback_speed(200));assert(tube_toggle_favorite("abcdefghijk")==1);
    show(entry);assert(last_count==2 && find("Restore this backup")==0 && strstr(last_items[0].overview,"pairing") && strstr(last_items[0].overview," UTC"));
    snprintf(stale,sizeof stale,"%s",row);assert(find("Delete this backup")==1);
    show(entry);assert(find("Restore this backup")==0 && strcmp(row,stale));snprintf(restore,sizeof restore,"%s",row);
    refused(stale);assert(tube_playback_speed()==200); /* an earlier ticket restores nothing */
    show(entry);assert(find("Restore this backup")==0);snprintf(restore,sizeof restore,"%s",row);
    p->list_catalog(restore,1,items_cb,NULL);assert(tube_playback_speed()==200); /* nor does a later page */
    show(restore);assert(!strcmp(last_items[0].title,"Backup restored") && strstr(last_items[0].subtitle,"-safety.json"));
    assert(tube_playback_speed()==150 && backups(&safety)==2 && safety==1);
    p->list_catalog("favorites",0,items_cb,NULL);assert(last_ok && !last_count);
    refused(restore);assert(backups(&safety)==2); /* reloading the finished action repeats nothing */
    /* What is saved next builds on the restored state, not on the replaced one. */
    assert(tube_set_playback_speed(125));p->shutdown();assert(!p->init() && tube_playback_speed()==125);
    p->list_catalog("favorites",0,items_cb,NULL);assert(last_ok && !last_count);
    /* The replaced state is listed as a safety copy; restoring it brings the favorite back. */
    show("settings:backup:list");assert(last_count==2);
    int copy_row=strstr(last_items[0].subtitle,"restore replaced")?0:1;assert(strstr(last_items[copy_row].subtitle,"restore replaced"));
    snprintf(entry,sizeof entry,"%s",last_items[copy_row].id);
    show(entry);assert(find("Restore this backup")==0 && strstr(last_items[0].overview,"restore replaced"));
    show(row);assert(!strcmp(last_items[0].title,"Backup restored") && tube_playback_speed()==200);
    p->list_catalog("favorites",0,items_cb,NULL);assert(last_ok && last_count==1 && !strcmp(last_items[0].id,"abcdefghijk"));
    /* Delete removes that one file. */
    assert(backups(&safety)==3);
    show(entry);assert(find("Delete this backup")==1);snprintf(erase,sizeof erase,"%s",row);
    show(erase);assert(!strcmp(last_items[0].title,"Backup deleted") && backups(&safety)==2);
    refused(erase);refused(entry);assert(backups(&safety)==2);
    /* Names that are not backups, unknown actions and bad periods change nothing. */
    const char *bad[]={"settings:backup:entry:../youtube.json","settings:backup:entry:youtube.json","settings:backup:entry:",
        "settings:backup:restore:1:youtube-20200101-000000.json","settings:backup:restore:x","settings:backup:delete:0:x",
        "settings:backup:frobnicate","settings:backup:auto:","settings:backup:auto:2","settings:backup:auto:7x","settings:backup:auto:-2"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i)refused(bad[i]);
    assert(backups(&safety)==2 && saved_period()==TUBE_BACKUP_AUTO_OFF);
    /* The period cycles off, daily, weekly, monthly and is saved for the device, not for a profile. */
    show("settings:backup:auto:1");assert(find("Automatic backup: daily")>=0 && !strcmp(row,"settings:backup:auto:7") && saved_period()==1);
    show(row);assert(find("Automatic backup: weekly")>=0 && !strcmp(row,"settings:backup:auto:30") && saved_period()==7);
    show(row);assert(find("Automatic backup: monthly")>=0 && !strcmp(row,"settings:backup:auto:-1") && saved_period()==30);
    show(row);assert(find("Automatic backup: off")>=0 && saved_period()==TUBE_BACKUP_AUTO_OFF);
    show("settings:backup:auto:1");
    SEPARATE_SETTINGS();show("library:profile:alice");show("settings:backup");assert(find("Automatic backup: daily")>=0);
    show("library:profile:guest");
    /* At startup a backup is made when none is recent enough; off makes none. */
    p->shutdown();assert(!p->init() && backups(&safety)==2); /* the newest one is from today */
    p->shutdown();wipe();assert(!backups(&safety));
    assert(!p->init() && backups(&safety)==1 && !safety);
    p->shutdown();assert(!p->init() && backups(&safety)==1);
    show("settings:backup");assert(find("Automatic backup: daily")>=0);
    show("settings:backup:auto:7");show("settings:backup:auto:30");show("settings:backup:auto:-1");
    p->shutdown();wipe();assert(!p->init() && !backups(&safety));
    /* A corrupt period reads as off. */
    p->shutdown();
    cJSON *store=read_store();cJSON_ReplaceItemInObject(store,"backup_auto_days",cJSON_CreateNumber(5));write_store(store);cJSON_Delete(store);
    assert(!p->init() && !backups(&safety));show("settings:backup");assert(find("Automatic backup: off")>=0);
    p->shutdown();
    store=read_store();cJSON_ReplaceItemInObject(store,"backup_auto_days",cJSON_CreateString("daily"));write_store(store);cJSON_Delete(store);
    assert(!p->init() && !backups(&safety));show("settings:backup");assert(find("Automatic backup: off")>=0);
    /* A file from before the setting existed starts without automatic backups. */
    p->shutdown();
    store=read_store();cJSON_DeleteItemFromObject(store,"backup_auto_days");write_store(store);cJSON_Delete(store);
    assert(!p->init() && !backups(&safety));
    p->shutdown();
    puts("PASS: backup folder, create, list, reviewed one-shot restore with reload, safety copy, delete, automatic period and startup backup");
}
