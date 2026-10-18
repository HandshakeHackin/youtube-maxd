/* Local family controls, independent of Google/Family Link. GPL-3.0-or-later. */
#ifndef MAXD_FAMILY_H
#define MAXD_FAMILY_H
#define MAXD_FAMILIES 16
#define MAXD_APPROVED_CHANNELS 32
typedef struct { char profile[33],name[81];int enabled,search,minutes;char channels[MAXD_APPROVED_CHANNELS][25];int count;int64_t day;double spent; } MaxdFamily;
static struct { char pin[TUBE_PASSWORD_RECORD+1];MaxdFamily profiles[MAXD_FAMILIES];int count,failures;long long unlocked,cooldown,last_tick,last_saved;char prompt[32]; } g_maxd_family;
static MaxdFamily *maxd_family_for(const char *profile,int create) {
    for(int i=0;i<g_maxd_family.count;++i)if(!strcmp(g_maxd_family.profiles[i].profile,profile))return &g_maxd_family.profiles[i];
    if(!create || g_maxd_family.count>=MAXD_FAMILIES)return NULL;
    MaxdFamily *f=&g_maxd_family.profiles[g_maxd_family.count++];memset(f,0,sizeof *f);snprintf(f->profile,sizeof f->profile,"%s",profile);return f;
}
static int maxd_family_restricted(const char *profile) {MaxdFamily *f=maxd_family_for(profile,0);return f && f->enabled;}
static int maxd_family_channel(const char *profile,const char *channel) {
    MaxdFamily *f=maxd_family_for(profile,0);if(!f || !f->enabled)return 1;
    for(int i=0;i<f->count;++i)if(!strcmp(f->channels[i],channel))return 1;
    return 0;
}
static int maxd_family_search(const char *profile) {MaxdFamily *f=maxd_family_for(profile,0);return !f || !f->enabled || f->search;}
static int maxd_family_parent(void) {return *g_maxd_family.pin && g_maxd_family.unlocked>now_ms();}
static void maxd_family_write(cJSON *root) {
    cJSON *family=cJSON_AddObjectToObject(root,"maxd_family");cJSON_AddStringToObject(family,"pin",g_maxd_family.pin);
    cJSON *rows=cJSON_AddArrayToObject(family,"profiles");
    for(int i=0;i<g_maxd_family.count;++i) {
        MaxdFamily *f=&g_maxd_family.profiles[i];cJSON *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"profile",f->profile);cJSON_AddStringToObject(o,"name",f->name);
        cJSON_AddBoolToObject(o,"enabled",f->enabled);cJSON_AddBoolToObject(o,"search",f->search);cJSON_AddNumberToObject(o,"minutes",f->minutes);
        cJSON_AddNumberToObject(o,"day",(double)f->day);cJSON_AddNumberToObject(o,"spent",ceil(f->spent));
        cJSON *channels=cJSON_AddArrayToObject(o,"channels");for(int j=0;j<f->count;++j)cJSON_AddItemToArray(channels,cJSON_CreateString(f->channels[j]));
        cJSON_AddItemToArray(rows,o);
    }
}
static void maxd_family_read(const cJSON *root) {
    memset(&g_maxd_family,0,sizeof g_maxd_family);const cJSON *family=cJSON_GetObjectItemCaseSensitive(root,"maxd_family");
    const char *pin=maxd_smart_string(family,"pin");if(tube_password_record_valid(pin))snprintf(g_maxd_family.pin,sizeof g_maxd_family.pin,"%s",pin);
    const cJSON *o,*rows=cJSON_GetObjectItemCaseSensitive(family,"profiles");if(!cJSON_IsArray(rows))return;
    cJSON_ArrayForEach(o,rows) {
        const char *profile=maxd_smart_string(o,"profile");if(!maxd_smart_text(profile,32,0) || maxd_family_for(profile,0))continue;
        MaxdFamily *f=maxd_family_for(profile,1);if(!f)break;
        const char *name=maxd_smart_string(o,"name");if(maxd_smart_text(name,80,1))snprintf(f->name,sizeof f->name,"%s",name);
        f->enabled=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o,"enabled"));f->search=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o,"search"));
        f->minutes=(int)maxd_smart_integer(o,"minutes",240,0);f->day=maxd_smart_integer(o,"day",1000000,0);
        f->spent=(double)maxd_smart_integer(o,"spent",86400,0);
        const cJSON *ch,*channels=cJSON_GetObjectItemCaseSensitive(o,"channels");if(!cJSON_IsArray(channels))continue;
        cJSON_ArrayForEach(ch,channels)if(f->count<MAXD_APPROVED_CHANNELS && cJSON_IsString(ch) && valid_channel(ch->valuestring))
            snprintf(f->channels[f->count++],25,"%s",ch->valuestring);
    }
}
static int maxd_family_prompt(const char *,char *,size_t);
static int maxd_family_catalog(const char *,int,evo_provider_items_cb,void *);
static int maxd_family_gate(const char *,int,evo_provider_items_cb,void *);
#endif
