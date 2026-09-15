#pragma once
#include "ini.h"
#include <math.h>
/* JSONL is flushed at the existing trainer log cadence, outside rollout steps.
 * Missing episode metrics stay absent; nonfinite metrics are explicit nulls. */
static void puf_json_string(FILE *f,const char *s) {
    fputc('"',f);
    for(const unsigned char *p=(const unsigned char*)s;*p;p++){
        if(*p=='"'||*p=='\\'){fputc('\\',f);fputc(*p,f);}
        else if(*p<32)fprintf(f,"\\u%04x",*p);
        else fputc(*p,f);
    }
    fputc('"',f);
}
static void puf_live_log(FILE *f,Dict *metrics) {
    if(!f)return;
    fputs("{\"type\":\"metrics\"",f);
    for(int i=0;i<metrics->size;i++){
        DictItem *item=&metrics->items[i];fputc(',',f);puf_json_string(f,item->key);fputc(':',f);
        if(isfinite(item->value))fprintf(f,"%.10g",item->value);else fputs("null",f);
    }
    fputs("}\n",f);fflush(f);
    if(ferror(f)){perror("live metrics write");exit(1);}
}
