/* plist_parse.c - Code about converting a plist file into a usable struct

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

#include <libxml/xmlreader.h>
#include <stdio.h>
#include "plist_parse.h"
#include <string.h>
#include <stdlib.h>

struct search_key_in_dict_ret{
	int found;
	char* value;
};
struct search_key_in_dict_ret* search_key_in_dict(xmlNode *dictNode, const char *targetKey) {
	struct search_key_in_dict_ret* ret=malloc(sizeof(struct search_key_in_dict_ret));
	ret->found=0;
	for (xmlNode *node = dictNode->children; node; node = node->next) {
		if (node->type == XML_ELEMENT_NODE && xmlStrEqual(node->name, (const xmlChar *)"key")) {
			xmlChar *keyContent = xmlNodeGetContent(node);
			if (keyContent && xmlStrEqual(keyContent, (const xmlChar *)targetKey)) {
				// Get the next element node (value)
				xmlNode *valueNode = node->next;
				while (valueNode && valueNode->type != XML_ELEMENT_NODE) {
					valueNode = valueNode->next;
				}

				if (valueNode &&( xmlStrEqual(valueNode->name, (const xmlChar *)"string") || xmlStrEqual(valueNode->name, (const xmlChar *)"integer")) ) {
					xmlChar *valueContent = xmlNodeGetContent(valueNode);
					ret->value=malloc(strlen((char*)valueContent)+1);
					memcpy(ret->value,valueContent,strlen((char*)valueContent)+1);
					ret->found=1;
					xmlFree(valueContent);
				}
			}
			xmlFree(keyContent);
		} else if (node->type == XML_ELEMENT_NODE && xmlStrEqual(node->name, (const xmlChar *)"dict")) {
			// Recurse into nested <dict>
			struct search_key_in_dict_ret* got=search_key_in_dict(node, targetKey);
			if(got->found==1){
				free(ret);
				return got;
			}else
				free(got);
		}
	}
	return ret;
}

int get_plist_value(xmlNode *dict_node,char *key_name,int64_t *write_result){
	struct search_key_in_dict_ret *key_search=search_key_in_dict(dict_node,key_name);
	if (key_search->found){
		sscanf(key_search->value,"%ld",write_result);
		free(key_search->value);
		free(key_search);
		return 0;
	}else{
		printf("failed to find \"%s\" in plist\n",key_name);
		free(key_search);
		return 1;
	}
}

struct ipdp_plist_info *parse_ipdp_plist(char *filename){
	LIBXML_TEST_VERSION;

	xmlDoc *doc = xmlReadFile(filename, NULL, 0);
	if(!doc){
		printf("Failed to parse plist file %s\n",filename);
		return NULL;
	}

	xmlNode *root = xmlDocGetRootElement(doc);
	if (!root) {
		printf("Empty plist document %s\n",filename);
		xmlFreeDoc(doc);
		return NULL;
	}

	//Search through all nodes
	xmlNode *dict_node=NULL;
	for (xmlNode *i = root->children; i ; i = i->next) {
		if (i->type == XML_ELEMENT_NODE && xmlStrEqual(i->name, (const xmlChar *)"dict")) {
			dict_node=i;
			break;
		}
	}
	if(dict_node==NULL){
		printf("Failed to find root dict node in plist file\n");
		return NULL;
	}

	struct ipdp_plist_info *ret=malloc(sizeof(struct ipdp_plist_info));

	if(get_plist_value(dict_node,"#page-bytes",&ret->page_bytes)){
		free(ret);
		return NULL;
	}
	if(get_plist_value(dict_node,"meta-per-logical-page",&ret->meta_per_logical_page)){
		free(ret);
		return NULL;
	}
	if(get_plist_value(dict_node,"#block-pages",&ret->block_pages)){
		free(ret);
		return NULL;
	}
	if(get_plist_value(dict_node,"#ce-blocks",&ret->ce_blocks)){
		free(ret);
		return NULL;
	}
	if(get_plist_value(dict_node,"#ce",&ret->ce)){
		free(ret);
		return NULL;
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return ret;
}
