#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "wc.h"
#include <ctype.h>

struct Hash_element{
    char* key;
    int val;
    struct Hash_element* next;
};

struct wc {
    struct Hash_element** element_array;
    int array_size;
    /* you can define this struct to have whatever fields you want. */
};
void element_destroy(struct Hash_element* element){
    if(element->next != NULL){
        element_destroy(element->next);
    }
    free(element->key);
    free(element);
    return;
}
unsigned long hash_function(char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++) != 0)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

struct Hash_element* Hash_element_init(char*key, int value){
        struct Hash_element* element = (struct Hash_element*)malloc(sizeof(struct Hash_element));
	element-> key = key;
        
        element->val = value;
        element->next = NULL;
	return element; 

}


void add_element(struct wc *wc, char *str){
    int hash_index;
    
    hash_index = hash_function(str)%10000;
    struct Hash_element* new_element = Hash_element_init(str, 1);
    if(wc->element_array[hash_index] == NULL){

            wc->element_array[hash_index] = new_element;
            return;

    }
    else{
        struct Hash_element*  current_element = wc->element_array[hash_index];
        struct Hash_element* prev_element = NULL;
        while(current_element != NULL){
            if(strcmp(current_element->key, str) == 0){
                current_element->val = current_element->val+1;
                element_destroy(new_element);
                return;
            }
            prev_element = current_element;
            current_element = current_element->next;  
        }
        prev_element->next = new_element;
        return;  
    }
}

void build_array(struct wc *wc, char* word_array){
    char *each_word = NULL;
    int i = 0;
    int word_size = 0;
    int last_word_loc;

    while(word_array[i] != '\0'){
       
        if(isspace(word_array[i])){
            if(word_size != 0){
               
                each_word = (char*)malloc((word_size+1)* sizeof(char));
                for(int j = 0; j< word_size; j++){
                    each_word[j] = word_array[last_word_loc+j];
                }
                each_word[word_size] = '\0';
                add_element(wc, each_word);
            }
            last_word_loc = i+1;
            word_size = 0;
            
        }  
        else{
            word_size++;
        }
        i++;
    }
}

struct wc *wc_init(char *word_array, long size)
{
	struct wc *wc;
    int count;
	wc = (struct wc *)malloc(sizeof(struct wc));
	assert(wc);

        
    for(int i = 0; i< size; i++){
       if(isspace(word_array[i]) != 0){
                  count = count + 1;
       }
        else{
            continue;
        }
    }
        
    if((count) > 0){
       count= count + 1; // count is used to deteremined the size of the hash_table array
    }
        
        wc->array_size = count;
        wc->element_array = (struct Hash_element **)malloc(wc->array_size * sizeof( struct Hash_element*));
        for(int i=0;i<wc->array_size;i++){
            wc->element_array[i]=NULL;
        }
        build_array(wc, word_array);
	return wc;
}



void wc_output(struct wc *wc)
{
    struct Hash_element* current_element;
    int size = wc->array_size;
        for(int i = 0; i<size; i++){
            if(wc->element_array[i] != NULL){
                current_element = wc->element_array[i];
                while(current_element != NULL){
                    printf("%s:%d\n", current_element->key, current_element->val);
                    current_element = current_element->next;
                }
            }
        }
}



void wc_destroy(struct wc *wc)
{
    for(int i = 0; i < wc->array_size; i++){
        if(wc->element_array[i] != NULL){
            element_destroy(wc->element_array[i]);
        }
    }
    free(wc->element_array);
    free(wc);
    return;
}






