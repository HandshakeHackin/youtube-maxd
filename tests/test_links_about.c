/* Link pages carry one address per row for the QR screen; the share rows of videos, channels
 * and playlists open them; About lists the version, links, feedback, credits and changes. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_qr_page.h"
#include "tube_about.h"
static const evo_provider_t *p;
static char row[160];
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void refused(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count);}
static int find(const char *title) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title)){snprintf(row,sizeof row,"%s",last_items[i].id);return i;}
    return -1;
}
/* The row of a paged menu, by title: its id is left in `row`. */
static int offered(const char *parent,const char *title) {
    int more=1;
    for(int page=0;more;++page) {
        p->list_catalog(parent,page,items_cb,NULL);assert(last_ok && !pending_count);
        if(find(title)>=0)return 1;
        more=last_more;
    }
    return 0;
}
/* Every row of a link page is an address the QR screen accepts and can encode. */
static void links(const char *parent,int count) {
    show(parent);assert(last_count==count && tube_qr_page_kind(parent)==TUBE_QR_PAGE_LINK);
    for(int i=0;i<last_count;++i) {
        char payload[TUBE_QR_PAYLOAD_SIZE];tube_qr_code code;
        assert(last_items[i].is_folder && *last_items[i].title);
        assert(tube_qr_row_payload(parent,last_items[i].id,last_items[i].title,last_items[i].overview,payload,sizeof payload)==1);
        assert(!strcmp(payload,last_items[i].overview) && !tube_qr_encode_text(payload,&code));
        for(int j=i+1;j<last_count;++j)assert(strcmp(last_items[i].id,last_items[j].id));
    }
}
static const char *listed="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":["
    "{\"id\":\"abcdefghijk\",\"title\":\"First\",\"subtitle\":\"Fixture channel\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"playback_context\":{\"playlist\":\"PLfixture01\",\"index\":0}},"
    "{\"id\":\"bcdefghijkl\",\"title\":\"Second\",\"channel_id\":\"not-a-channel\"}],\"has_more\":false}}";
int main(void) {
    char dir[]="/tmp/tube-links-XXXXXX",title[96],old[96];unsigned scope=0;
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("playlist:PLfixture01",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job(listed);assert(last_ok && last_count==2);
    /* A video's Share link opens its link page: video and embed, plus its channel and the playlist it was listed in. */
    assert(offered("account:menu:abcdefghijk","Share link") && !strcmp(row,"link:video:abcdefghijk"));
    links("link:video:abcdefghijk",4);
    assert(!strcmp(last_items[0].title,"Video link") && !strcmp(last_items[0].overview,"https://youtu.be/abcdefghijk"));
    assert(!strcmp(last_items[0].subtitle,last_items[0].overview)); /* the address is readable without the code */
    assert(!strcmp(last_items[1].title,"Embed link") && !strcmp(last_items[1].overview,"https://www.youtube.com/embed/abcdefghijk"));
    assert(!strcmp(last_items[2].title,"Channel link") && !strcmp(last_items[2].overview,"https://www.youtube.com/channel/UCaaaaaaaaaaaaaaaaaaaaaa"));
    assert(!strcmp(last_items[3].title,"Playlist link") && !strcmp(last_items[3].overview,"https://www.youtube.com/playlist?list=PLfixture01"));
    show(last_items[3].id);assert(last_count==1 && !strcmp(last_items[0].id,"account:notice")); /* a link row opens nothing */
    assert(offered("account:menu:bcdefghijkl","Share link"));links(row,2);
    assert(!strcmp(last_items[1].overview,"https://www.youtube.com/embed/bcdefghijkl"));
    /* The share id of earlier versions lists the same links. */
    assert(offered("account:menu:abcdefghijk","Play next") && sscanf(row,"local:video:%u:",&scope)==1);
    snprintf(old,sizeof old,"local:video:%u:share:abcdefghijk",scope);show(old);
    assert(last_count==4 && !strcmp(last_items[0].overview,"https://youtu.be/abcdefghijk"));
    refused("local:video:0:share:abcdefghijk");
    /* Channels and playlists share from their own menus. */
    assert(!strcmp(tube_folder_menu("channel:UCaaaaaaaaaaaaaaaaaaaaaa","Fixture channel"),"settings:pin:channel:UCaaaaaaaaaaaaaaaaaaaaaa"));
    show("settings:pin:channel:UCaaaaaaaaaaaaaaaaaaaaaa");assert(find("Share link")>=0 && !strcmp(row,"link:channel:UCaaaaaaaaaaaaaaaaaaaaaa"));
    links(row,1);assert(!strcmp(last_items[0].title,"Channel link") && !strcmp(last_items[0].overview,"https://www.youtube.com/channel/UCaaaaaaaaaaaaaaaaaaaaaa"));
    show("settings:pin:playlist:PLfixture01");assert(find("Share link")>=0 && !strcmp(row,"link:playlist:PLfixture01"));
    links(row,1);assert(!strcmp(last_items[0].title,"Playlist link") && !strcmp(last_items[0].overview,"https://www.youtube.com/playlist?list=PLfixture01"));
    show("library:profile:fixture");
    show("playlist:menu:PLfixture01");assert(find("Share link")>=0 && !strcmp(row,"link:playlist:PLfixture01"));
    tube_folder_menu("library:playlist:PLfixture01","Mine");show("settings:pin:library:playlist:PLfixture01");
    assert(find("Share link")>=0 && !strcmp(row,"link:playlist:PLfixture01"));
    show("library:profile:guest");
    /* Nothing malformed becomes a link, and a link page has one page. */
    const char *bad[]={"link:","link:video:","link:video:short","link:video:abcdefghij!","link:channel:UCshort","link:channel:XXaaaaaaaaaaaaaaaaaaaaaa",
        "link:playlist:x","link:playlist:PL bad id","link:other","link:about:more","link:video:abcdefghijk:extra",
        "link:playlist:PLaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i)refused(bad[i]);
    p->list_catalog("link:video:abcdefghijk",1,items_cb,NULL);assert(!last_ok);
    p->list_catalog("link:about",1,items_cb,NULL);assert(!last_ok);
    /* About: reached from YouTube preferences under the name and version. */
    assert(!tube_about_title(title,sizeof title) && strstr(title,"ProsperoTube ")==title && strlen(title)>13);
    assert(offered("settings",title) && !strcmp(row,"settings:about"));
    show("settings:about");assert(last_count==5 && !strcmp(last_items[0].title,title) && strstr(last_items[0].subtitle,"GPL-3.0-or-later"));
    assert(!strcmp(last_items[1].id,"link:about") && !strcmp(last_items[2].id,"link:feedback"));
    assert(!strcmp(last_items[3].id,"settings:about:credits") && !strcmp(last_items[4].id,"settings:about:changelog"));
    links("link:about",3);
    for(int i=0;i<3;++i) {
        const tube_about_link *link=tube_about_link_at(i);
        assert(link && !link->feedback && !strcmp(last_items[i].title,link->title) && !strcmp(last_items[i].overview,link->url));
        assert(!strcmp(last_items[i].subtitle,link->detail));
    }
    links("link:feedback",1);
    assert(tube_about_link_at(3)->feedback && !strcmp(last_items[0].overview,tube_about_link_at(3)->url) && !tube_about_link_at(4));
    /* Credits and the installed version's changes, page by page. */
    int total=0,more=1;
    for(int page=0;more;++page) {
        p->list_catalog("settings:about:credits",page,items_cb,NULL);assert(last_ok && last_count<=24);
        for(int i=0;i<last_count;++i,++total) {
            const tube_about_credit *credit=tube_about_credit_at(total);
            assert(credit && !strcmp(last_items[i].title,credit->name) && !strncmp(credit->detail,last_items[i].subtitle,strlen(last_items[i].subtitle)));
        }
        more=last_more;
    }
    assert(total==tube_about_credit_count() && total>=10);
    const char *version=tube_about_change_count(tube_about_version())?tube_about_version():NULL;
    total=0;more=1;
    for(int page=0;more;++page) {
        p->list_catalog("settings:about:changelog",page,items_cb,NULL);assert(last_ok && last_count<=24);
        for(int i=0;i<last_count;++i,++total) {
            const tube_about_change *change=tube_about_change_at(version,total);
            assert(change && *last_items[i].title && !strncmp(change->text,last_items[i].title,strlen(last_items[i].title)));
            assert(strstr(last_items[i].subtitle,change->version));
        }
        more=last_more;
    }
    assert(total==tube_about_change_count(version) && total>0);
    p->list_catalog("settings:about:changelog",50,items_cb,NULL);assert(last_ok && !last_count && !last_more);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: link pages for videos, channels, playlists and project links; share rows; About version, links, feedback, credits and changelog");
}
