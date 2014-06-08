#include <stdio.h>
#include <stdlib.h>
#include "simos.h"


// We need to build a timer list, in sorted order
// only the first event in the list will be checked to see 
// whether its time is up
// To make insertion efficient, we keep a binary tree of events
// while using a eventHead pointer to point to the leftmost node 
//
// eventNode is defined to keep track of timer events and 
// to maintain the event tree (and list)
// The fields: time, pid, act, recurP belong to the timer event level
// The fields: left, right, parent belong to the tree data structure level

struct eventNode
{ int time;   // in number of instruction cycles, relative to absolute time
  int pid;    // if action = actReady, put pid in  ready queue
              // for other actions pid is ignored (can be set to 0)
  int act;    // action to be performed when timer expires
  int recurP; // if it is not a recurring timer, then this is 0; 
              // else this is the recurring period
  struct eventNode *left, *right;
  struct eventNode *parent;
              // keep parent node to make removal of head node easier
} *eventTree, *eventHead;


// the event tree has a dummy node to begin with, with the highest time
void initialize_eventtree ()
{ 
  eventTree = (struct eventNode *) malloc (sizeof (struct eventNode));
  eventTree->time = maxCPUcycles + 1;
  eventTree->pid = 0;
  eventTree->act = 0;
  eventTree->recurP = 0;
  eventTree->left = NULL;
  eventTree->right = NULL;
  eventTree->parent = NULL; 
  eventHead = eventTree;
}

void insert_event (event)
struct eventNode *event;
{ struct eventNode *cnode;

  event->left = NULL;
  event->right = NULL;
  
  cnode = eventTree;
  while (cnode != NULL)
  { if (event->time < cnode->time) 
      if (cnode->left == NULL)
      { cnode->left = event;
        event->parent = cnode;
        if (eventHead == cnode) eventHead = event;
         // the new event has a lower time, eventHead should point to it
        break;
      }
      else cnode = cnode->left;
    else // event->time >= tree->time
    { if (cnode->right == NULL)
      { cnode->right = event;
        event->parent = cnode;
        break;
      }
      else cnode = cnode->right;
    }
  }
}

// only remove the eventHead, which is always leftmost node in the tree
// first update the eventHead, then update the tree
// note: freeing the node is not done here, recurring event reuses node 

void remove_eventhead ()
{ struct eventNode *event, *temp;

  event = eventHead;
  if (event->right != NULL)
  { temp = event->right;
    while (temp->left != NULL) temp = temp->left;
    eventHead = temp;
    event->right->parent = event->parent;
  }
  else eventHead = event->parent;
  event->parent->left = event->right;
}

// recursive call, external caller calls with (eventTree)
void list_events (event)
struct eventNode *event;
{
  if (event != NULL)
  { printf ("  Event: Time: %d, Pid: %d, Action: %d, RecurP: %d, ",
             event->time, event->pid, event->act, event->recurP);
    if (event->left != NULL) printf ("left=%d, ", event->left->time);
    else printf ("left=null, ");
    if (event->right != NULL) printf ("right=%d\n", event->right->time);
    else printf ("right=null\n");
    list_events (event->left);
    list_events (event->right);
  }
}

void dump_events ()
{ 
        printf ("* ---------------------- Event List --------------------------\n");
	list_events (eventTree);
	printf ("  EventHead: Time: %d, Pid: %d, Action: %d, RecurP: %d\n",
           eventHead->time, eventHead->pid, eventHead->act, eventHead->recurP);
        printf ("* ------------------------------------------------------------\n");
}

// never use this call, only to check whether the eventHead is set correctly
// void test_eventhead()
// { 
//   while (eventHead != eventTree)
//   { printf ("Event head: time=%d, pid=%d, action=%d, recurP=%d\n",
//              eventHead->time, eventHead->pid,
//              eventHead->act, eventHead->recurP);
//     remove_eventhead ();
//   }
// }


//=============================================================
// high level timer calls
//

void initialize_timer ()
{ 
  initialize_eventtree();
}

genericPtr add_timer (time, pid, action, recurperiod)
int time, pid, action, recurperiod; // time is from current time
{ struct eventNode *event;

  time = CPU.numCycles + time;
  if (time > maxCPUcycles)
  { printf ("timer exceeds CPU cycle limit!!!\n"); exit(-1); }
  else 
  { event = malloc (sizeof (struct eventNode));
    event->time = time;
    event->pid = pid;
    event->act = action;
    event->recurP = recurperiod;
    insert_event (event);
if (Debug) printf ("* Set timer: Time: %d, Pid: %d, Action: %d, RecurP: %d\n",
                    event->time, event->pid, event->act, event->recurP);
    return ((genericPtr) event);
      // to not expose the eventNode structure, a casted pointer is returned
  }
}

void check_timer ()
{ struct eventNode *event;

  while (eventHead->time <= CPU.numCycles)
  { 
	event = eventHead;
	if (Debug)
	{ 
		printf ("\n* Process event: Time = %d, Pid = %d, Action =%d, RecurP = %d\n", event->time, event->pid, event->act, event->recurP);
		printf ("* Check Timer: Interrupt:  %x ==> ", CPU.interruptV);
	}
    switch (event->act)
    { case actTQinterrupt:
        set_interrupt (tqInterrupt);
        break;
      case actAgeInterrupt:
        set_interrupt (ageInterrupt);
        break;
      case actReadyInterrupt:
        insert_doneWait_process (event->pid);
        set_interrupt (doneWaitInterrupt);
      case actNull:
        if (Debug) printf ("Event: time=%d, pid=%d, action=%d, recurP=%d\n",
                          event->time, event->pid, event->act, event->recurP);
        break;
      default:
        printf ("Encountering an illegitimate action code\n");
        break;
    }
    remove_eventhead ();
    if (event->recurP > 0) // recurring event, put the event back
    { event->time = CPU.numCycles + event->recurP;
      if (event->time > maxCPUcycles)
      { printf ("timer exceeds CPU cycle limit!!!\n"); exit(-1); }
      else insert_event (event);
    }
    else free (event);
if (Debug) { printf ("Interrupt: %x\n", CPU.interruptV); /*dump_events (); printf("\n");*/}
  }
}

// deactivate event uses eventNode ptr to get to the target event node
// but the event node may have just been freed, a risk!!!
//   (when cpu execution terminates for various reasons and timer is also up)
void deactivate_timer (castedevent)
genericPtr castedevent;
{ struct eventNode *event;

  event = (struct eventNode *) castedevent;
  event->act = actNull;
if (Debug) 
{ printf ("* Deactivate Event: Addr: %x, Time: %d, Pid: %d, Action: %d, RecurP: %d\n",
             castedevent, event->time, event->pid, event->act, event->recurP);
//  dump_events ();
}
}


