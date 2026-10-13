#include "arch/x86_64/rtc.h"
#include "arch/x86_64/irq_state.h"
#include <stdint.h>

typedef struct { uint8_t day, month, year, status; } sample_t;
static uint8_t in_port(uint16_t port){
    uint8_t result;
    __asm__ volatile("inb %1, %0":"=a"(result):"Nd"(port));
    return result;
}
static void out_port(uint16_t port,uint8_t value){
    __asm__ volatile("outb %0, %1"::"a"(value),"Nd"(port));
}
static uint8_t register_read(uint8_t reg){
    out_port(0x70,(uint8_t)(reg|0x80));
    return in_port(0x71);
}
static bool sample_read(sample_t *s){
    /* Don't spin forever on a malfunctioning CMOS/virtual RTC. */
    unsigned tries=100000;
    while((register_read(0x0a)&0x80) && --tries){}
    if(!tries)return false;
    s->day=register_read(0x07);
    s->month=register_read(0x08);
    s->year=register_read(0x09);
    s->status=register_read(0x0b);
    return !(register_read(0x0a)&0x80);
}
static unsigned bcd(uint8_t v){return (unsigned)(v>>4)*10u+(unsigned)(v&15u);}
static unsigned days(unsigned y,unsigned m){
    static const unsigned lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(m<1||m>12)return 0;
    if(m==2&&((y%4==0&&y%100!=0)||y%400==0))return 29;
    return lengths[m-1];
}
bool rtc_read_date(rtc_date_t *date){
    if(!date)return false;
    uint64_t flags=irq_save_disable();
    uint8_t selector=in_port(0x70);
    sample_t a={0},b={0};
    bool valid=sample_read(&a)&&sample_read(&b);
    out_port(0x70,selector);
    irq_restore(flags);
    if(!valid||a.day!=b.day||a.month!=b.month||a.year!=b.year||a.status!=b.status)return false;
    unsigned day=(a.status&4)?a.day:bcd(a.day);
    unsigned month=(a.status&4)?a.month:bcd(a.month);
    unsigned year=(a.status&4)?a.year:bcd(a.year);
    /* Conventional PC RTC two-digit year, used only for a prototype calendar.
       80..99 map to the 1900s, 00..79 to the 2000s. */
    year+=(year>=80)?1900u:2000u;
    if(!day||day>days(year,month))return false;
    date->day=day;date->month=month;date->year=year;
    return true;
}
