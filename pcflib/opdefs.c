#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <search.h>
#include <errno.h>
#include <string.h>
#include "opdefs.h"

struct PCFGate _gate;

void nop(struct PCFState * st, struct PCFOP * op)
{}

void initbase_op(struct PCFState * st, struct PCFOP * op)
{
  ENTRY ent, * r;
  ent.key = "main";
  if(hsearch_r(ent, FIND, &r, st->labels) == 0)
        {
          fprintf(stderr, "Problem searching hash table for main: %s\n", strerror(errno));
          abort();
        }
 
  uint32_t * target = r->data;
  st->PC = *target;

  st->base = *((uint32_t*)op->data);
}

void mkptr_op(struct PCFState * st, struct PCFOP * op)
{
  uint32_t idx = *((uint32_t*)op->data);
  assert(st->wires[idx + st->base].flags == KNOWN_WIRE);
  st->wires[idx + st->base].value += st->base;
}

void const_op(struct PCFState * st, struct PCFOP * op)
{
  struct const_op_data * data = op->data;
  uint32_t idx = data->dest + st->base;
  st->wires[idx].value = data->value;

  if(st->wires[idx].keydata != 0)
    st->delete_key(st->wires[idx].keydata);
  st->wires[idx].keydata = 0;
  st->wires[idx].flags = KNOWN_WIRE;
}

void bits_op(struct PCFState * st, struct PCFOP * op)
{
  struct bits_op_data * data = op->data;
  uint32_t s_idx = data->source + st->base;
  uint32_t i = 0, cval;

  assert(st->wires[s_idx].flags == KNOWN_WIRE);

  cval = st->wires[s_idx].value;

  for(i = 0; i < data->ndests; i++)
    {
      st->wires[data->dests[i] + st->base].value = (cval & 0x01);
      st->wires[data->dests[i] + st->base].flags = KNOWN_WIRE;

      if(st->wires[data->dests[i] + st->base].keydata != 0)
        st->delete_key(st->wires[data->dests[i] + st->base].keydata);

      st->wires[data->dests[i] + st->base].keydata = st->copy_key(st->constant_keys[cval & 0x01]);

      cval = cval >> 1;
    }
}

void call_op (struct PCFState * st, struct PCFOP * op)
{
  struct call_op_data * data = (struct call_op_data*)op->data;

  ENTRY * ent = data->target;
  ENTRY * r = 0;

  if(strcmp(data->target->key, "alice") == 0)
    {
      uint32_t i = 0, idx = 0;
      // Get the argument to this function
      if(st->inp_i == 0)
        {
          for(i = 1; i <= 32; i++)
            {
              idx = idx << 1;
              assert(st->wires[st->base + data->newbase - i].value < 2);
              idx += st->wires[st->base + data->newbase - i].value;
            }
          st->inp_idx = idx;
        }

      if(st->inp_i < 32)
        {
          i = st->inp_i;
          st->inp_i++;
          st->input_g.wire1 = st->inp_idx + i;
          st->input_g.wire2 = st->inp_idx + i;
          st->input_g.reswire = st->base + data->newbase + i;
          st->input_g.truth_table = 5;
          st->input_g.tag = TAG_INPUT_A;

          if(st->wires[st->input_g.reswire].keydata != 0)
            st->delete_key(st->wires[st->input_g.reswire].keydata);

          st->wires[st->input_g.reswire].keydata = st->copy_key(st->callback(st, &st->input_g));
          st->wires[st->input_g.reswire].flags = UNKNOWN_WIRE;
          st->curgate = &st->input_g;
          // Not yet done with function call
          st->PC--;
        }
      else
        {
          st->inp_i = 0;
        }
    }
  else if(strcmp(data->target->key, "bob") == 0)
    {
      uint32_t i = 0, idx = 0;
// Get the argument to this function
      if(st->inp_i == 0)
        {
          for(i = 1; i <= 32; i++)
            {
              idx = idx << 1;
              assert(st->wires[st->base + data->newbase - i].value < 2);
              assert(st->wires[st->base + data->newbase - i].flags == KNOWN_WIRE);
              idx += st->wires[st->base + data->newbase - i].value;
            }
          st->inp_idx = idx;
        }

      if(st->inp_i < 32)
        {
          i = st->inp_i;
          st->inp_i++;
          st->input_g.wire1 = st->inp_idx+i;
          st->input_g.wire2 = st->inp_idx+i;
          st->input_g.reswire = st->base + data->newbase + i;
          st->input_g.truth_table = 5;
          st->input_g.tag = TAG_INPUT_B;
          if(st->wires[st->input_g.reswire].keydata != 0)
            st->delete_key(st->wires[st->input_g.reswire].keydata);

          st->wires[st->input_g.reswire].keydata = st->copy_key(st->callback(st, &st->input_g));
          st->wires[st->input_g.reswire].flags = UNKNOWN_WIRE;
          st->curgate = &st->input_g;
          // Not yet done with function call
          st->PC--;
        }
      else
        {
          st->inp_i = 0;
        }
    }
  else if(strcmp(data->target->key, "output_alice") == 0)
    {
      uint32_t i;
      if(st->inp_i < 32)
        {
          i = st->inp_i;
          st->inp_i++;
          st->input_g.wire1 = st->base + data->newbase - (32 - i);
          st->input_g.wire2 = st->base + data->newbase - (32 - i);
          st->input_g.reswire = st->base + data->newbase - (32 - i);
          st->input_g.truth_table = 5;
          st->input_g.tag = TAG_OUTPUT_A;
          st->callback(st, &st->input_g);
          st->curgate = &st->input_g;
          st->PC--;
        }
      else
        st->inp_i = 0;
    }
  else if(strcmp(data->target->key, "output_bob") == 0)
    {
      uint32_t i;
      if(st->inp_i < 32)
        {
          i = st->inp_i;
          st->inp_i++;
          st->input_g.wire1 = st->base + data->newbase - (32 - i);
          st->input_g.wire2 = st->base + data->newbase - (32 - i);
          st->input_g.reswire = st->base + data->newbase - (32 - i);
          st->input_g.truth_table = 5;
          st->input_g.tag = TAG_OUTPUT_B;
          st->callback(st, &st->input_g);
          st->curgate = &st->input_g;
          st->PC--;
        }
      else
        st->inp_i = 0;
    }
  else
    {
      struct activation_record * newtop = malloc(sizeof(struct activation_record));
      check_alloc(newtop);

      newtop->rest = st->call_stack;
      newtop->ret_pc = st->PC;
      st->call_stack = newtop;

      if(hsearch_r(*ent, FIND, &r, st->labels) == 0)
        {
          fprintf(stderr, "Problem searching hash table for %s: %s\n", ent->key, strerror(errno));
          abort();
        }
 
      long * target = r->data;
      st->PC = *target;
      st->base += data->newbase;
    }
}

void gate_op(struct PCFState * st, struct PCFOP * op)
{
  struct PCFGate * data = (struct PCFGate*)op->data;
  uint32_t op1idx = data->wire1 + st->base;
  uint32_t op2idx = data->wire2 + st->base;
  uint32_t destidx = data->reswire + st->base;
  uint8_t bits[4];
  int8_t i = 0;
  uint8_t tab = data->truth_table;
  for(i = 0; i < 4; i++)
    {
      bits[i] = tab & 0x01;
      tab = tab >> 1;
    }

  assert(st->curgate == 0);
  assert(data->truth_table < 16);


  if(st->wires[destidx].keydata != 0)
    st->delete_key(st->wires[destidx].keydata);
  st->wires[destidx].keydata = 0;

  if((st->wires[op1idx].flags != KNOWN_WIRE) || (st->wires[op2idx].flags != KNOWN_WIRE))
    {
      // Time for the callback
      assert((st->wires[op1idx].keydata != 0) && (st->wires[op2idx].keydata != 0));

      st->curgate = &_gate;

      st->curgate->wire1 = op1idx;
      st->curgate->wire2 = op2idx;
      st->curgate->reswire = destidx;
      st->curgate->truth_table = data->truth_table;
      st->curgate->tag = TAG_INTERNAL;
      
      st->wires[destidx].keydata = st->copy_key(st->callback(st, st->curgate));
      st->wires[destidx].flags = UNKNOWN_WIRE;
    }
  else
    {
      // Check that we are dealing only with bits
      assert((st->wires[op1idx].value < 2) && (st->wires[op2idx].value < 2));
      assert(((st->wires[op1idx].value) + (2*(st->wires[op2idx].value))) < 4);
      if((bits[(st->wires[op1idx].value) + (2*(st->wires[op2idx].value))]) >= 2)
        fprintf(stderr, "Problem!\n");
      assert((bits[(st->wires[op1idx].value) + (2*(st->wires[op2idx].value))]) < 2);
      st->wires[destidx].keydata = st->copy_key(st->constant_keys[bits[(st->wires[op1idx].value) + (2*(st->wires[op2idx].value))]]);
      st->wires[destidx].value = bits[(st->wires[op1idx].value) + (2*(st->wires[op2idx].value))];
      st->wires[destidx].flags = KNOWN_WIRE;
    }
}

void copy_op(struct PCFState * st, struct PCFOP * op)
{
  struct copy_op_data * data = (struct copy_op_data*)op->data;
  uint32_t i;
  uint32_t dest = data->dest + st->base;
  uint32_t source = data->source + st->base;
  for(i = 0; i < data->width; i++)
    {
      if(st->wires[dest+i].keydata != 0)
        st->delete_key(st->wires[dest+i].keydata);

      if(st->wires[source+i].keydata != 0)
        st->wires[dest+i].keydata = st->copy_key(st->wires[source+i].keydata);
      else
        st->wires[dest+i].keydata = 0;

      st->wires[dest+i].value = st->wires[source+i].value;
      st->wires[dest+i].flags = st->wires[source+i].flags;
    }
}

void indir_copy_op(struct PCFState * st, struct PCFOP * op)
{
  struct copy_op_data * data = (struct copy_op_data*)op->data;
  uint32_t dest = st->wires[data->dest + st->base].value;
  uint32_t source = data->source + st->base;
  uint32_t i;
  for(i = 0; i < data->width; i++)
    {
      if(st->wires[dest+i].keydata != 0)
        st->delete_key(st->wires[dest+i].keydata);

      if(st->wires[source+i].keydata != 0)
        st->wires[dest+i].keydata = st->copy_key(st->wires[source+i].keydata);
      else
        st->wires[dest+i].keydata = 0;

      st->wires[dest+i].value = st->wires[source+i].value;
      st->wires[dest+i].flags = st->wires[source+i].flags;
    }
}

void copy_indir_op(struct PCFState * st, struct PCFOP * op)
{
  struct copy_op_data * data = (struct copy_op_data*)op->data;
  uint32_t dest = data->dest + st->base;
  uint32_t source = st->wires[data->source + st->base].value;
  uint32_t i;
  for(i = 0; i < data->width; i++)
    {
      if(st->wires[dest+i].keydata != 0)
        st->delete_key(st->wires[dest+i].keydata);

      if(st->wires[source+i].keydata != 0)
        st->wires[dest+i].keydata = st->copy_key(st->wires[source+i].keydata);
      else
        st->wires[dest+i].keydata = 0;

      st->wires[dest+i].value = st->wires[source+i].value;
      st->wires[dest+i].flags = st->wires[source+i].flags;
    }

}

void ret_op(struct PCFState * st, struct PCFOP * op)
{
  struct activation_record * rec = st->call_stack;

  if(st->call_stack == 0)
    st->done = -1;
  else
    {
      st->call_stack = rec->rest;
      st->PC = rec->ret_pc;
      free(rec);
    }
}
