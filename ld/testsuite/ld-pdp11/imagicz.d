#name: pdp11-aout imagic format -z
# nm sort alphabetically since both _start and _data are 0
#source: sections.s
#ld: -z
#DUMPPROG: nm
#...
0+2 B _bss
#...
0+0 D _data
#...
0+0 T _start
#pass
