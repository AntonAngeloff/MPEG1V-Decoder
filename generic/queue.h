#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

#include "mmfutil.h"

/* TODO: Reimplement
 */

typedef struct queue_node {
    queue_node *next;
    void* data;
} queue_node;

void queue_enqueue(queue_node **root, void *data) {
    if((*root) == NULL) {
        (*root) = mmf_alloc(sizeof(queue_node);)
        (*root)->data = n;
        (*root)->next = NULL;
        return;
    }

    queue_node *p = *root;

    //Find last node
    while(p->next) {
        p = p->next;
    }

    //Add new node at the end
    p->next = mmf_alloc(sizeof(queue_node);)
    p->next->data = n;
    p->next->next = NULL;
}

void queue_extract(queue_node **root, void **value)
{
    if((*root) == NULL) {
        //Queue empty
        return;
    }

    assert((*value) != NULL);

    queue_node *p = *root;

    //Delete first element
    *root = *root->next;

    //Get value of first element
    (*value) = p->data;
    free(p);
}

void queue_free(queue_node **root)
{
    queue_node *p = *root;

    while(p) {
        queue_node *tmp = p;

        p = p->next;
        mmf_free(p);
    }
}

#endif // QUEUE_H_INCLUDED
