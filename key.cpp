//#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <linux/input.h>
#include <time.h>

#include <string>

#define VENDORID  0x08ff
#define PRODUCTID 0x0009

int16_t fd = -1;
FILE *pFile;

char path[300];
char barcode[255];

int init_keyboard(unsigned int VENDOR,unsigned int PRODUCT) {
    fd=-1;
  int count=0;
  struct dirent **files=NULL;
  struct input_id id;

  count = scandir("/dev/input",&files,0,0)-1;
  while(count>=0) {
    if (fd==-1 && strncmp(files[count]->d_name,"event", 5)==0) {
      sprintf(path,"/dev/input/%s",files[count]->d_name);
      fd = open(path,O_RDONLY);
      if(fd>=0) {
        if(ioctl(fd,EVIOCGID,(void *)&id)<0 ) perror("ioctl EVIOCGID");
        else {
          if(id.vendor==VENDOR && id.product==PRODUCT && id.bustype==BUS_USB) { printf("Device found at %s ",path); }
          else {
            close(fd);
            fd = -1;
          }
        }
      } else {
        fprintf(stderr,"Error opening %s:\t",path); perror("");
      }
    }
    free(files[count--]);
  }
  free(files);

  int grab=1;
  if(fd>=0) ioctl(fd,EVIOCGRAB,&grab);
  else {
      fprintf(stderr,"Device not found or access denied. ");
      return EXIT_FAILURE;
  }
  return 0;
}


char remap_codes(int scancode){
  char r=' ';
  switch(scancode) {
    // NumLock = off + N     // NumLock = on
    case 0x1: r='1'; break;
    case 0x2: r='2'; break;
    case 0x3: r='3'; break;
    case 0x4: r='4'; break;
    case 0x5: r='5'; break;
    case 0x6: r='6'; break;
    case 0x7: r='7'; break;
    case 0x8: r='8'; break;
    case 0x9: r='9'; break;
    case 0xA: r='0'; break;
    //case 0x1B: r='E'; break; // Enter
    default:
    //printf("Scancode:0x%X - [%c]\n",scancode,(unsigned char)scancode);
    //r=(unsigned char)scancode;
    r=' ';
  }
  return r;
}

char read_keyboard() {
  memset(barcode,0,sizeof barcode);
  char carcodepart; 
  struct input_event ev;
  ssize_t n;

  while(true) {
    n = read(fd, &ev, sizeof (struct input_event));
    if(n == (ssize_t)-1) {
        if (errno == EINTR) continue;
        else break;
    } else if(n != sizeof ev) {
        errno = EIO;
        break;
    }

    if( ev.type==EV_KEY && ev.value==EV_KEY ) {
    //printf("  %X\n",ev.code-1);
    
    //printf("%X\n",remap_codes(ev.code-1));
    carcodepart = remap_codes(ev.code-1);
    strncat(barcode,&carcodepart,1);
    //std::cout << barcode <<"\n";
    //printf("%X\n",ev.code);
	    if (ev.code==0x1C) {
	    	printf("%s\n",barcode);
	    	printf("%s\n",&carcodepart);
	    	printf("\n");
	    	return 0x1C; //barcode;
	    }
    }
    //if( ev.type==EV_KEY && ev.value==EV_KEY ) return remap_codes(ev.code);
  }
  return '!';
}


int connect_keyboard(int argc,char* arg) {
    char name[256] = "Unknown";
    if(argc>1) {
    // пробуем отсканировать VEND:PROD
    unsigned int VEND=VENDORID,PROD=PRODUCTID;
    if(2==sscanf(arg,"%x:%x",&VEND,&PROD)) {
        printf("\nConnecting to device %04x:%04x... ",VEND,PROD);
        if(init_keyboard(VEND,PROD)!=0) { printf("FAILED\n"); return(1); }
        printf("OK\n");
    } else {
        // пробуем найти path
        snprintf(path,200,"%s",arg);
        printf("\nConnecting to device %s ... ",path);
        if((fd = open(path,O_RDONLY)) == -1) { printf("FAILED\n"); return(1); }
        printf("OK\n");
    }
    } else { // откроем дефолтный девайс
    printf("\nConnecting to default device %04x:%04x... ",VENDORID,PRODUCTID);
        if(init_keyboard(VENDORID,PRODUCTID)!=0) { printf("FAILED\n"); return(1); }
    printf("OK\n");
    }
    ioctl(fd, EVIOCGNAME (sizeof (name)), name);
    fprintf(stderr, "Reading from: %s (%s)\n", path, name);
    return 0;
}


int main(int argc, char* argv[]) {

    printf("Runnung... (Example: \"sudo ./keyboardnoid 08ff:0009 \"echo \\\"%%c\\\" >> /tmp/111.txt\")");

    char r;
    char action[300]="notify-send -t 300 \"KeyCode: %c\"";

    if(argc>2) snprintf(action,1024,"%s",argv[2]); // скопировать строку акции

    // коннектимся
    while(1) { if(0 == connect_keyboard(argc,argv[1])) break; sleep(2); } // 1 сек

    while(true) {
        if(!(r=read_keyboard())) continue;
 
     	//printf("+Scancode:0x%X - [%c]\n",r,(unsigned char)r);

    	if(r=='!') { // обрыв связи
        	sleep(1);
        	close(fd); printf("port_disconnected\n");
        	snprintf(path,100,action,'?'); system(path); // подать сигнал
        	while(true) { if(0 == connect_keyboard(argc,argv[1])) break; sleep(2); } // 10 сек
        	snprintf(path,100,action,'!'); system(path); // подать сигнал что все в порядке
        	continue;
    	}

    	//if(r==0x0D || r==0x0A || r=='"' || r=='\'') r='@'; // не надо нам энтеров и кавычек!

    	
    	//snprintf(path,100,action,r);
    	//printf("ACTION: `%s`\n",path);
    	//system(path);
    	pFile=fopen("/tmp/barcodes.txt", "at");
    	if(pFile==NULL) {
    		perror("Error opening file.");
		}
    	else {
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
    		fprintf(pFile, "%d-%02d-%02d %02d:%02d:%02d,%s \n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,barcode);
    		fclose(pFile);
    		}
    }
}


