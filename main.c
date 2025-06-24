#include <stdio.h>
#include <stdlib.h>
#include "plist_parse.h"
#include <unistd.h>

#define VERSION "v0.0-dev"

void help(char* progname){
	printf(
			"Usage: %s [options]  \n"
			"Options:\n"
			"  -h                  Print the program version and exit successfully\n"
			"  -p <plist>          Supply the plist file\n"
			"  -i <ipdp file>      Supply an iphone dataprotection nand dump file\n"
			"  -v                  Print the version and exit successfully\n"
			"  -V                  Print verbose information\n"
			, progname);
}


void print_value(char* name, long int value,int tabsize){
	tabsize-=printf("%s",name);
	for(int i=tabsize;i>0;i--){
		putchar(' ');
	}
	printf("%ld\n",value);
}

int main(int argc, char *argd[]){
	char *plist_file=NULL,*ipdp_filename=NULL;
	int opt;

	int verbose=0;
	while ((opt = getopt(argc, argd, "hp:vVi:")) != -1) {
		switch (opt) {
			case 'h':
				help(argd[0]);
				return 0;
			case 'v':
				printf("%s\n",VERSION);
				return 0;
			case 'p':
				plist_file=optarg;
				break;
			case 'V':
				verbose=1;
				break;
			case 'i':
				ipdp_filename=optarg;
				break;
			default:
				help(argd[0]);
				return 1;
		}
	}

	if(!plist_file){
		printf("No plist file provided\n");
		return 1;
	}
	if(!ipdp_filename){
		printf("No image file provided\n");
		return 1;
	}

	//FILE *image_file1=fopen(argd[1],"r");
	struct ipdp_plist_info *plist_info=parse_ipdp_plist(plist_file);

	long int DumpPageSize=plist_info->page_bytes+plist_info->meta_per_logical_page+8;
	long int calculated_file_size=DumpPageSize*plist_info->block_pages*plist_info->ce_blocks*plist_info->ce;
	if ( plist_info && verbose){
		printf("## Data from the plist:\n");
#		define TABSIZE 35
		print_value("Bytes per page:",plist_info->page_bytes,TABSIZE);
		print_value("Metadata bytes per logical page:",plist_info->meta_per_logical_page,TABSIZE);
		print_value("(calculated) dump page size",DumpPageSize,TABSIZE);
		print_value("Block Pages",plist_info->block_pages,TABSIZE);
		print_value("CE Blocks",plist_info->ce_blocks,TABSIZE);
		print_value("CEs",plist_info->ce,TABSIZE);
	}


	FILE *ipdp_file=fopen(ipdp_filename,"r");

	fseek(ipdp_file, 0L, SEEK_END);
	long int filesize = ftell(ipdp_file);
	fseek(ipdp_file, 0L, SEEK_SET);

	if (verbose&&filesize==calculated_file_size){
		printf("Iphone dataprotection nand file size is correct!\n");
	}else if(filesize!=calculated_file_size){
		printf("Error: Iphone dataprotection nand file size is not correct!\n");
	}

	uint8_t *IPDP_Page_dump=malloc(DumpPageSize);

	uint64_t ECC_error_count=0,Blank_page_count=0,Correct_page_count=0,Other_pages=0;

	//image files structure :
	//
	// The file :  [ dump_page ] [ dump_page ] .... [ dump page ]
	// dump_page : [ page_bytes (8192 bytes probably) ] [ meta_per_logical_page (12/16 bytes probably) ] [ ret1 (uint32_t) ] [ ret2 (uint32_t) ]
	uint32_t page_count=plist_info->block_pages*plist_info->ce_blocks*plist_info->ce;
	for(uint32_t i=0;i<page_count;i++){
		fread(IPDP_Page_dump,DumpPageSize,1,ipdp_file);
		// from IOFlashControllerUserClient_OutputStruct in original iphone dataprotection ramdisk code
		uint32_t ret1=*(uint32_t*)(IPDP_Page_dump+DumpPageSize-8),ret2=*(uint32_t*)(IPDP_Page_dump+DumpPageSize-4);
#		define PRINT_HEADER printf("CE:%c Page:0x%04x ",(i%2==0)?'0':'1',i/2)
		if(verbose)
			PRINT_HEADER;
		if(ret1==0&&ret2==0){
			if(verbose)
				printf("Correctly read page\n");
			Correct_page_count++;
		}else if(ret1==0xE00002D1 && ret2==0){
			if(verbose)
				printf("reported ECC error\n");
			ECC_error_count++;
		}else if(ret1==0xE00002E5 && ret2==0){
			if(verbose)
				printf("reported unformatted media (erased page?)\n");
			Blank_page_count++;
		}else{
			if(!verbose)
				PRINT_HEADER;
			printf("unknown values %04x:%04x\n",ret1,ret2);
			Other_pages++;
		}
	}

	//printf("ECC error pages=%lu\nBlank pages=%lu\nCorrectly red pages=%lu\n",ECC_error_count,Blank_page_count,Correct_page_count);
	print_value("ECC error pages:",ECC_error_count,23);
	print_value("Blank pages:",Blank_page_count,23);
	print_value("Correct_page_count",Correct_page_count,23);
	print_value("Other pages",Other_pages,23);
	print_value("Total pages",page_count,23);

	free(IPDP_Page_dump);

	free(plist_info);
}
