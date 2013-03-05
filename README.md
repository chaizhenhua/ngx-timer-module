ngx-timer-module
================
#Synopsis

Ngx-timer-module is an enhance module for nginx, it provides extra timer algorithms: binary heap(```heap```), quad heap(```heap4```), orignal rbtree(```rbtree```) and timer wheel(```wheel```).

#Installation

```
$ cd /path/to/nginx
$ patch -p1 < /path/to/ngx-timer-module/ngx_timer_module.patch
$ ./configure --add-module=/path/to/ngx-timer-module/
$ make
$ make install

```

#Directives

##timer\_use
**Syntax:**  **timer\_use** *heap|heap4|rbtree|wheel*  
**Default:**	if timer_resolution is set timer\_use wheel, else timer\use heap  
**Context:**	main  

Choose the timer algorithms.

Example:
```timer_use heap;```

##timer\_wheel\_max
 
**Syntax:**  **timer\_wheel\_max** *interval*  
**Default:**   4m  
**Context:**   main    

Example:
```
timer_resolution 100ms;
timer_wheel_max 4m;
timer_use wheel;
```


