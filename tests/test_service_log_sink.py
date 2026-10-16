"""Run the actual embedded host_write callback without console hardware."""
from pathlib import Path
import subprocess
import tempfile
source = (Path(__file__).resolve().parents[1] / 'standalone/src/tube_engine.c').read_text()
start = source.index('static JSValue host_write(')
function = source[start:source.index('\n}', start) + 2]
scaffold = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#define TUBE_SERVICE_EMBEDDED 1
#define JS_EXCEPTION -1
#define JS_UNDEFINED 0
typedef void JSContext;
typedef const char *JSValueConst;
typedef int JSValue;
static char logged[512];
static const char *JS_ToCStringLen(JSContext *c,size_t *n,const char *s) {(void)c;*n=strlen(s);return s;}
static void JS_FreeCString(JSContext *c,const char *s) {(void)c;(void)s;}
static void evo_boot_log(const char *fmt,...) {va_list a;va_start(a,fmt);vsnprintf(logged,sizeof logged,fmt,a);va_end(a);}
"""
checks = r"""
int main(void) {
 const char *safe="YouTube operation failed: stage=execute action=history category=TypeError";
 assert(host_write(NULL,NULL,1,&safe,1)==0);
 assert(strcmp(logged,"service: YouTube operation failed: stage=execute action=history category=TypeError")==0);
 const char *browse="YouTube browse shape: tiles=2";host_write(NULL,NULL,1,&browse,1);
 assert(strcmp(logged,"service: YouTube browse shape: tiles=2")==0);
 const char *phone="YouTube phone state: channel ready";host_write(NULL,NULL,1,&phone,1);
 assert(strcmp(logged,"service: YouTube phone state: channel ready")==0);
 const char *probe="YouTube media probe: phase=header attempt=1 status=403";host_write(NULL,NULL,1,&probe,1);
 assert(strcmp(logged,"service: YouTube media probe: phase=header attempt=1 status=403")==0);
 logged[0]=0;host_write(NULL,NULL,1,&probe,0);assert(!logged[0]);
 logged[0]=0;host_write(NULL,NULL,1,&safe,0);assert(!logged[0]);
 const char *private="private exception token";host_write(NULL,NULL,1,&private,1);assert(!logged[0]);
 char large[300];memset(large,'a',sizeof large);memcpy(large,safe,strlen(safe));large[299]=0;
 const char *oversized=large;host_write(NULL,NULL,1,&oversized,1);assert(!logged[0]);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as d:
 c=Path(d)/'sink.c';binary=Path(d)/'sink';c.write_text(scaffold+function+checks)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(c),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
print('PASS: actual embedded log callback persists bounded diagnostics and excludes other output')
