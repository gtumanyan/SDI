#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

char buf[1024*1024*10];
char infs[1024][1024];
int n=0;
int sz=0;

int main(int argc, char* argv[])
{
    FILE *f;
    char *p=buf;
    char *t,*e;
    int i;
    FILE *g;

    memset(buf,0,sizeof(buf));

    f=fopen(argv[1],"rb");
    fseek(f,0,SEEK_END);
    sz=ftell(f);
    fseek(f,0,SEEK_SET);
    fread(buf,1,sz,f);

    while(p)
    {
        p=strchr(p+1,'$');
        if(p)
        {
            t=strstr(p,"Filter:");
            if(t&&strchr(p+1,'$')>t)
            {
                e=strchr(t,'\r');

                sprintf(infs[n],"%.*s",e-t,t);
                for(i=0;i<n;i++)
                    if(strcmpi(infs[i],infs[n])==0)break;

                if(i==n)
                {
                    //printf("+%s\n",infs[n]);
                    n++;
                }
                else
                {
                    int len;
                    len=(strstr(p,"\r\n\r\n")<strstr(p,"}manager"))?strstr(p,"\r\n\r\n")-p+2:t-p+e-t;
                    //printf("%d\n",len);
                    p-=2+2;len+=2+2;
                    //printf("-%s\n",infs[n]);
                    printf("<%.*s>",len,p);
                    memmove(p,p+len,1024*1024*5);
                }
            }
        }
    }
    fclose(f);

    g=fopen(argv[2],"wb");
    fwrite(buf,1,strlen(buf),g);
    fclose(g);
    return 0;
}
