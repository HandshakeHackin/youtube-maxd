/* PS5-hosted, opt-in LAN party room. GPL-3.0-or-later. */
#ifndef MAXD_PARTY_SERVER_H
#define MAXD_PARTY_SERVER_H
#define MAXD_PARTY_ITEMS 64
typedef struct {char id[12],title[121];unsigned votes;int approved,rejected,enqueued;} MaxdPartyItem;
int maxd_party_start(const char *address,unsigned port,long long now);
void maxd_party_stop(void);
void maxd_party_tick(long long now);
int maxd_party_running(void);
const char *maxd_party_address(void);
const char *maxd_party_join(void);
unsigned maxd_party_revision(void);
int maxd_party_count(void);
const MaxdPartyItem *maxd_party_item(int index);
int maxd_party_approve(int index,int approve);
void maxd_party_mark_queued(int index);
int maxd_party_discover(char *address,unsigned size);
#endif
