#include "word.h"
#include "./BSP/SRAM/sram.h"
#include "./BSP/SDIO/sdio_sdcard.h"
#include "ff.h"			
#include "diskio.h"		
#include "./FATFS/exfuns/exfuns.h"
#include "./SYSTEM/sys/sys.h"


extern FIL fil;
extern UINT res;
extern uint8_t flag;
char target_word[20]={0};
uint8_t length=0;

char word[20]={0};
char meaning[100]={0};
char buffer[500];
uint32_t data_offset=0;
uint32_t  word_offset[1000]={0},page[1000]={0};
uint32_t word_quantity=0,word_len=0,nlen=0;
uint32_t len = 0;
uint32_t list_len = 0,target_number=0;
char word_list[8][20]={0};
uint8_t word_group=0,offset_group=0,pages=1,page_number=0;

void sddata_to_flash()
{

	int word_count=0;
    uint32_t  len=0;
	UINT res;
	while(1)
	{
			while(1)
			{
				res=f_read(&fil,buffer+len,1,&br);
				if(!res&&br==1)
				{
						nlen++;
						if(*(buffer+len)=='\n')
						{
							data_offset=nlen;
						}
						if(buffer[len-1] == ',')
						{
							break;
						}
						len++;

				}
				else 
				{
				flag=0;	
				sram_write((uint8_t*)word_offset, 4000*word_group, 4*(word_quantity%1000));
				return;
				}
			}
			word_offset[word_count]=data_offset;
			len=0;
			word_count++;
			word_quantity++;
			if(word_count==999)
				break;
     }
	sram_write((uint8_t*)word_offset, 4000*word_group, 4000);
	word_group++;
	 
}


void flash_to_offset(uint8_t j)
{
if(j<word_quantity/1000)
{
	sram_read((uint8_t*)word_offset, 0+4000*j, 4000);
}
else
{
	sram_read((uint8_t*)word_offset, 0+4000*j,4*(word_quantity%1000));
}
sprintf(word, "%u", word_offset[0]);
printf(word);
printf("\r\n");
}


void data_to_word(uint16_t i)
{
	UINT res;
	uint8_t len=0;
	memset(word, 0, sizeof(word));
	memset(meaning, 0, sizeof(meaning));
	
	if(offset_group!=i/1000)
	{
	offset_group=i/1000;
	flash_to_offset(offset_group);
	}
	
	i=i%1000;
			f_lseek(&fil,word_offset[i]);
			while(1)
			{
				res=f_read(&fil,word+len,1,&br);
				if(!res||br==1)
				{
					if(word[len] == ',')
					{
						word[len] = '\n';
						break;
					}
					len++;
				}
			}
			len=0;
		while(1)
			{
				res=f_read(&fil,meaning+len,1,&br);
				if(!res||br==1)
				{
					if(f_tell(&fil)==word_offset[i+1])
					{
						break;
					}
					len++;
				}
			}
}
void offset_to_wordlist()
{
	len = 0;
    nlen = 0;
    word_len = 0;
    
    printf("Starting wordlist generation...\n");
    
    for(int i = 0; i < 1000; i++)
    {
        if(word_offset[i] == 0 && i > 0) break;
        
        len = 0;
        memset(word, 0, sizeof(word));
        
        f_lseek(&fil, word_offset[i]);
        
// 读取单个单词，直到遇到逗号
        while(1)
        {
            
            res = f_read(&fil, word + len, 1, &br);
            if(!res && (br == 1))
            {
                if(word[len] == ',')
                {
                    word[len] = '\n';  
                    break;
                }
                len++;
            }
            else break;
        }
        

        if(len > 0) {
            sram_write((uint8_t*)word, 50000 + word_len, ++len);
            word_len += len;
            nlen++;
			if(nlen==8)
			{
			    nlen=0;
				page[pages]=word_len;
				pages++;
			}
        }
		
    }
    
// 在字符串数据末尾补充一个结束符
    uint8_t null_byte = 0;
    sram_write(&null_byte, 50000 + word_len, 1);
    

}

void sram_to_word_list(uint8_t p)
{
	memset(word_list, 0, sizeof(word_list));
	list_len=page[p];
	
    for(int i=0;i<8;i++)
	{
	  word_len=0;
	  while(1)
	  {
		word_list[i][word_len]=sram_test_read(50000+list_len);
		if(word_list[i][word_len]=='\n')
		{
		word_list[i][word_len]=' ';
		list_len++;
		break;
		}
		list_len++;
		word_len++;
	   }
	
	}

}

uint32_t find_word()
{
	uint32_t word_len=0;
	char a;
	int i;
	a='\n';
	target_number=0;
	sram_write((uint8_t*)&a, 49999, 1);
	while(1)
	{
	a=sram_test_read(49999+word_len);
	if(a=='\n'&&target_number<word_quantity-2)
	{
		word_len++;
		for(i=0;i<length;i++)
		{
		a=sram_test_read(49999+word_len);
		if(target_word[i]!=a)
			break;
		word_len++;
		}
		if(i==length)
		{

		a=sram_test_read(49999+word_len);
		if(a=='\n')
		return target_number;
		}
		target_number++;
	}
	word_len++;

	
	}

}
