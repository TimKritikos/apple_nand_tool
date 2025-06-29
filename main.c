/* main.c

   This file is part of the apple nand tool project.

   Copyright (c) 2025 Efthymios Kritikos

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <stdlib.h>
#include "plist_parse.h"
#include <unistd.h>
#include <string.h>

#define VERSION "v0.0-dev"

void help(char* progname){
	printf(
			"Usage: %s [options]  \n"
			"Options:\n"
			"  -h                  Print the program version and exit successfully\n"
			"  -p <plist>          Supply the plist file\n"
			"  -i <ipdp file>      Supply an iphone dataprotection nand dump file\n"
			"  -I <ipdp file>      Supply the second iphone dataprotection nand dump file to merge the correct pages\n"
			"  -o <ipdp file>      Supply the output iphone dataprotection file\n"
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

struct ipdp_stats_t{
	uint64_t ECC_error_count;
	uint64_t Blank_page_count;
	uint64_t Correct_page_count;
	uint64_t Other_pages;
	uint64_t page_count;
};

#define RET1_ECC_ERR 0xE00002D1
#define RET1_BLANK   0xE00002E5

#define RET1_SKIPPED 0x32489122

struct ipdp_stats_t *ipdp_get_stats(struct ipdp_plist_info *plist_info, FILE *dump, int verbose,int DumpPageSize, long int calculated_file_size){

	fseek(dump, 0L, SEEK_END);
	long int filesize = ftell(dump);
	fseek(dump, 0L, SEEK_SET);

	if (verbose&&filesize==calculated_file_size){
		printf("Iphone dataprotection nand file size is correct!\n");
	}else if(filesize!=calculated_file_size){
		printf("Error: Iphone dataprotection nand file size is not correct!\n");
		return NULL;
	}

	struct ipdp_stats_t *ret=malloc(sizeof(struct ipdp_stats_t));

	memset(ret,0,sizeof(struct ipdp_stats_t));

	ret->page_count=plist_info->block_pages*plist_info->ce_blocks*plist_info->ce;

	uint8_t *IPDP_Page_dump=malloc(DumpPageSize);

	int spot_file=0;

	for(uint32_t i=0;i<ret->page_count;i++){
		if(fread(IPDP_Page_dump,DumpPageSize,1,dump)!=1){
			printf("Failed to read input file\n");
			free(IPDP_Page_dump);
			free(ret);
			return NULL;
		}

		// from IOFlashControllerUserClient_OutputStruct in original iphone dataprotection ramdisk code
		uint32_t ret1=*(uint32_t*)(IPDP_Page_dump+DumpPageSize-8),ret2=*(uint32_t*)(IPDP_Page_dump+DumpPageSize-4);

#		define PRINT_HEADER printf("CE:%c Page:0x%04x ",(i%2==0)?'0':'1',i/2)
		if(verbose)
			PRINT_HEADER;
		if(ret1==0&&ret2==0){
			if(verbose)
				printf("Correctly read page\n");
			ret->Correct_page_count++;
		}else if(ret1==RET1_SKIPPED && ret2==0){
			if(spot_file==0){
				printf("Detected spot file\n");
				spot_file=1;
			}
			ret->Correct_page_count++;
		}else if(ret1==RET1_ECC_ERR && ret2==0){
			if(verbose)
				printf("reported ECC error\n");
			ret->ECC_error_count++;
		}else if(ret1==RET1_BLANK && ret2==0){
			if(verbose)
				printf("reported unformatted media (erased page?)\n");
			ret->Blank_page_count++;
		}else{
			if(!verbose)
				PRINT_HEADER;
			printf("unknown values %04x:%04x\n",ret1,ret2);
			ret->Other_pages++;
		}
	}

	free(IPDP_Page_dump);

	return ret;
}

struct ipdp_merge_stats_t{
	uint64_t Mismatching_correct_pages;
	uint64_t ECC_on_just_one;
	uint64_t Blank_on_just_one;
	uint64_t ECC_error_count;
	uint64_t Blank_pages;
	uint64_t Other_pages;
	uint64_t page_count;
};

struct ipdp_merge_stats_t *ipdp_merge(struct ipdp_plist_info *plist_info, FILE *ipdp_file1 ,FILE *ipdp_file2, FILE *out, int verbose,int DumpPageSize, long int calculated_file_size,FILE* dump_ecc_log_file){


	fseek(ipdp_file1, 0L, SEEK_END);
	long int filesize1 = ftell(ipdp_file1);
	fseek(ipdp_file1, 0L, SEEK_SET);

	fseek(ipdp_file2, 0L, SEEK_END);
	long int filesize2 = ftell(ipdp_file2);
	fseek(ipdp_file2, 0L, SEEK_SET);

	if (verbose&&filesize2==calculated_file_size && filesize1==calculated_file_size ){
		printf("Iphone dataprotection nand file1 size is correct!\n");
		printf("Iphone dataprotection nand file2 size is correct!\n");
	}else if(filesize2!=calculated_file_size || filesize1!=calculated_file_size){
		printf("Error: one of the Iphone dataprotection nand file's size is not correct!\n");
		return NULL;
	}

	struct ipdp_merge_stats_t *ret=malloc(sizeof(struct ipdp_merge_stats_t));

	memset(ret,0,sizeof(struct ipdp_merge_stats_t));

	ret->page_count=plist_info->block_pages*plist_info->ce_blocks*plist_info->ce;

	uint8_t *IPDP_Page_dump1=malloc(DumpPageSize);
	uint8_t *IPDP_Page_dump2=malloc(DumpPageSize);

	int spot_file=0;
	int warned_both_skipped=0;

	unsigned long int log_file_index=0;

	for(uint32_t i=0;i<ret->page_count;i++){
		if( (fread(IPDP_Page_dump1,DumpPageSize,1,ipdp_file1)!=1) || (fread(IPDP_Page_dump2,DumpPageSize,1,ipdp_file2)!=1) ){
			printf("Failed to read on of the input files\n");
			free(IPDP_Page_dump1);
			free(IPDP_Page_dump2);
			free(ret);
			return NULL;
		}

		// from IOFlashControllerUserClient_OutputStruct in original iphone dataprotection ramdisk code
		uint32_t dump1_ret1=*(uint32_t*)(IPDP_Page_dump1+DumpPageSize-8),dump1_ret2=*(uint32_t*)(IPDP_Page_dump1+DumpPageSize-4);
		uint32_t dump2_ret1=*(uint32_t*)(IPDP_Page_dump2+DumpPageSize-8),dump2_ret2=*(uint32_t*)(IPDP_Page_dump2+DumpPageSize-4);

		int use_page=1;

		if(dump1_ret2!=0||dump2_ret2!=0){
			//Unexpected stuff. We shouldn't get here ever
			if(dump2_ret1==0&&dump2_ret2==0)
				use_page=2;
			else if(dump1_ret1!=0||dump1_ret2!=0){
				ret->Other_pages++;
			}
		}else if(dump1_ret1==0&&dump2_ret1==0){
			//Wen both dumps are reported fine on this page.
			if(memcmp(IPDP_Page_dump1,IPDP_Page_dump2,DumpPageSize)!=0){
				ret->Mismatching_correct_pages++;
			}
		}else if(dump1_ret1==0||dump2_ret1==0){
			//one of the dumps is fine
			if(dump2_ret1==0){ //by default we use 1 so we only check if it isn't
				use_page=2;
				if(dump1_ret1==RET1_ECC_ERR)
					ret->ECC_on_just_one++;
				else
					ret->Blank_on_just_one++;
			}else{
				/*just for reporting*/
				if(dump2_ret1==RET1_ECC_ERR)
					ret->ECC_on_just_one++;
				else
					ret->Blank_on_just_one++;
			}
		}else if(dump1_ret1==RET1_SKIPPED||dump2_ret1==RET1_SKIPPED){
			if(spot_file==0){
				spot_file=1;
				printf("Detected spot file\n");
			}
			if(dump1_ret1==RET1_SKIPPED)
				use_page=2;
			if(dump1_ret1==RET1_SKIPPED&&dump2_ret1==RET1_SKIPPED){
				if(!warned_both_skipped){
					printf("Warning: found at least one page that is skipped on both files\n");
					warned_both_skipped=1;
				}
			}else if(dump1_ret1!=0||dump2_ret1!=0){
				fprintf(dump_ecc_log_file,"spot_list[%ld].ce=%d;\nspot_list[%ld].page=0x%04x;\n",log_file_index,(i%2==0)?0:1,log_file_index,i/2);
				log_file_index++;
				ret->ECC_error_count++;
			}
		}else{
			//both dumps are bad in this page, let's see what to report..
			if(dump_ecc_log_file){
				fprintf(dump_ecc_log_file,"spot_list[%ld].ce=%d;\nspot_list[%ld].page=0x%04x;\n",log_file_index,(i%2==0)?0:1,log_file_index,i/2);
				log_file_index++;

			}
			if(dump1_ret1==RET1_ECC_ERR||dump2_ret1==RET1_ECC_ERR){
				if(dump2_ret1==RET1_ECC_ERR)
					use_page=2;
				ret->ECC_error_count++;
			}else if(dump1_ret1==RET1_BLANK && dump2_ret1==RET1_BLANK){
				ret->Blank_pages++;
			}else
				ret->Other_pages++;

		}

		if(use_page==1)
			fwrite(IPDP_Page_dump1,DumpPageSize,1,out);
		else
			fwrite(IPDP_Page_dump2,DumpPageSize,1,out);
		if((i%100)==0){
			printf("\r%lu%%",(i*100)/ret->page_count);
		}
	}
	printf("\n");

	free(IPDP_Page_dump1);
	free(IPDP_Page_dump2);

	return ret;
}

int main(int argc, char *argd[]){
	char *plist_file=NULL,*ipdp_filename=NULL,*ipdp_filename2=NULL,*ipdp_output=NULL,*dump_ecc_log_filename=NULL;
	int opt;

	int verbose=0;
	while ((opt = getopt(argc, argd, "hp:vVi:I:o:l:")) != -1) {
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
			case 'I':
				ipdp_filename2=optarg;
				break;
			case 'o':
				ipdp_output=optarg;
				break;
			case 'l':
				dump_ecc_log_filename=optarg;
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
	if (!plist_info)
		return 1;

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
	if(ipdp_file==NULL){
		printf("couldn't open first iphone dataprotection file\n");
		free(plist_info);
		return 1;
	}

	if(ipdp_filename2!=NULL){
		if(ipdp_output==NULL){
			printf("ERROR: need output file to be set\n");
			fclose(ipdp_file);
			free(plist_info);
			return 1;
		}
		FILE *ipdp_file2=fopen(ipdp_filename2,"r");
		if(ipdp_file2==NULL){
			printf("Couldn't open second iphone dataprotection file\n");
			fclose(ipdp_file);
			free(plist_info);
			return 1;
		}else{
			FILE *out=fopen(ipdp_output,"w");
			if(out==NULL){
				printf("couldn't open output file\n");
				fclose(ipdp_file2);
				fclose(ipdp_file);
				free(plist_info);
			}

			FILE *dump_ecc_log_file=NULL;
			if(dump_ecc_log_filename!=NULL)
				dump_ecc_log_file=fopen(dump_ecc_log_filename,"w");

			struct ipdp_merge_stats_t *stats= ipdp_merge(plist_info,ipdp_file,ipdp_file2,out,verbose,DumpPageSize,calculated_file_size,dump_ecc_log_file);
			if(stats!=NULL){
				print_value("Both correct but mismatching pages(!):",stats->Mismatching_correct_pages,40);
				print_value("ECC error on just one page :",stats->ECC_on_just_one,40);
				print_value("One page blank the other correct (!):",stats->Blank_on_just_one,40);
				print_value("Both pages have ECC error:",stats->ECC_error_count,40);
				print_value("Both pages are blank:",stats->Blank_pages,40);
				print_value("Both pages have unkown error(!):",stats->Other_pages,40);
				print_value("Total pages:",stats->page_count,40);
				free(stats);
			}
			fclose(ipdp_file2);
			fclose(ipdp_file);
			fclose(out);
		}
	}else{
		struct ipdp_stats_t *stats= ipdp_get_stats(plist_info,ipdp_file,verbose,DumpPageSize,calculated_file_size);
		if(stats){
			print_value("ECC error pages:",stats->ECC_error_count,23);
			print_value("Blank pages:",stats->Blank_page_count,23);
			print_value("Correct_page_count",stats->Correct_page_count,23);
			print_value("Other pages",stats->Other_pages,23);
			print_value("Total pages",stats->page_count,23);
			free(stats);
		}
		fclose(ipdp_file);
	}


	free(plist_info);
}
