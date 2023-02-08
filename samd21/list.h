/*
 * list.h
 *
 * Created: 15/03/2021 20:55:48
 *  Author: jonso
 */ 


#ifndef LIST_H_
#define LIST_H_


typedef struct LIST_Node
{
	struct LIST_Node *Succ;
	struct LIST_Node *Pred;
} LIST_Node_t;

typedef struct LIST_List
{
	LIST_Node_t *Head;
	LIST_Node_t *Tail;
	LIST_Node_t *TailPred;
} LIST_List_t;


__inline static void LIST_Init(LIST_List_t *List)
{
	List->Head = (LIST_Node_t *)&List->Tail;
	List->Tail = 0;
	List->TailPred = (LIST_Node_t *)&List->Head;
}

__inline static bool LIST_IsEmpty(LIST_List_t *List)
{
	return (List->TailPred == (LIST_Node_t *)List);
}

__inline static LIST_Node_t *LIST_NextNode(LIST_Node_t *Node)
{
	return Node->Succ;
}

__inline static LIST_Node_t *LIST_HeadNode(LIST_List_t *List)
{
	if (!LIST_IsEmpty(List))
		return List->Head;
	else
		return NULL;
}

__inline static LIST_Node_t *LIST_TailNode(LIST_List_t *List)
{
	if (!LIST_IsEmpty(List))
		return List->TailPred;
	else
		return NULL;
}

__inline static void LIST_AddNodeToHead(LIST_List_t *List, LIST_Node_t *Node)
{
    /*
	Make the node point to the old first node in the list and to the
	head of the list.
    */
    Node->Succ = List->Head;
    Node->Pred = (LIST_Node_t *)&List->Head;

    /*
	New we come before the old first node which must now point to us
	and the same applies to the pointer to-the-first-node in the
	head of the list.
    */
    List->Head->Pred = Node;
    List->Head = Node;
}

__inline static void LIST_AddNodeToTail(LIST_List_t *List, LIST_Node_t *Node)
{

    /*
	Make the node point to the head of the list. Our predecessor is the
	previous last node of the list.
    */
    Node->Succ	       = (LIST_Node_t *)&List->Tail;
    Node->Pred	       = List->TailPred;

    /*
	Now we are the last now. Make the old last node point to us
	and the pointer to the last node, too.
    */
    List->TailPred->Succ = Node;
    List->TailPred	     = Node;
}

__inline static void LIST_RemoveNode(LIST_Node_t *Node)
{
    /*
	Just bend the pointers around the node, ie. we make our
	predecessor point to our successor and vice versa
    */
    Node->Pred->Succ = Node->Succ;
    Node->Succ->Pred = Node->Pred;
}

__inline static LIST_Node_t *LIST_RemoveNodeFromHead(LIST_List_t *List)
{
    /* Get the first node of the list */
    LIST_Node_t *Node = LIST_HeadNode(List);
    if (Node)
	    LIST_RemoveNode(Node);

    /* Return it's address or NULL if there was no node */
    return Node;
}

__inline static LIST_Node_t *OS_ListRemoveTail(LIST_List_t *List)
{
    /* Get the last node of the list */
	LIST_Node_t *Node = LIST_TailNode(List);
    if (Node)
		LIST_RemoveNode(Node);

    /* Return it's address or NULL if there was no node */
    return Node;
}



#endif /* LIST_H_ */