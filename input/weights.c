/*
 *  weights.c:		Input of weights
 *
 *  Written by:		Ullrich Hafner
 *		
 *  This file is part of FIASCO ([F]ractal [I]mage [A]nd [S]equence [CO]dec)
 *  Copyright (C) 1994-2000 Ullrich Hafner <hafner@bigfoot.de>
 */

/*
 *  $Date: 2000/06/14 20:50:13 $
 *  $Author: hafner $
 *  $Revision: 5.1 $
 *  $State: Exp $
 */

#include "config.h"

#include "types.h"
#include "macros.h"
#include "error.h"

#include "bit-io.h"
#include "arith.h"
#include "rpf.h"
#include "misc.h"

#include "weights.h"

/* Enable checkpoint logging for weight reading (ground truth) */
#define ORIG_WEIGHT_CHECKPOINTS_ENABLED 1

#ifdef ORIG_WEIGHT_CHECKPOINTS_ENABLED
#define CP_READ_WEIGHT_ORIG(state, label, edge, domain, weight) \
    fprintf(stderr, "[ORIG CP_READ_WEIGHT] state=%u label=%u edge=%u domain=%d weight=%.6f\n", \
            (state), (label), (edge), (int)(domain), (weight))
#else
#define CP_READ_WEIGHT_ORIG(state, label, edge, domain, weight) ((void)0)
#endif

/*****************************************************************************

				public code
  
*****************************************************************************/

void
read_weights (unsigned total, wfa_t *wfa, bitfile_t *input)
/*
 *  Read #'total' weights from input stream 'input' and
 *  update transitions of the WFA states with corresponding weights.
 *
 *  No return value.
 *
 *  Side effects:
 *	'wfa->weights' are filled with the decoded values
 */
{
   unsigned	    state;
   unsigned	    label;
   unsigned	    edge;		/* current edge */
   unsigned	   *weights_array;	/* array of weights to encode */
   unsigned	   *level_array;	/* array of corresponding levels */
   unsigned	    offset1, offset2;	/* prob. model offsets. */
   unsigned	    offset3, offset4;	/* prob. model offsets. */
   bool_t	    delta_approx = NO; 	/* true if delta has been used */
   
   /*
    *  Check whether delta approximation has been used
    */
   for (state = wfa->basis_states; state < wfa->states; state++)
      if (wfa->delta_state [state])
      {
	 delta_approx = YES;
	 break;
      }
  
   /*
    *  Generate array of corresponding levels (context of probability model)
    */
   {
      int 	min_level, max_level; 	/* min and max range level */
      int 	d_min_level, d_max_level; /* min and max range level (delta) */
      unsigned *lptr;			/* pointer to current corresp. level */
      int	domain;			/* current domain */
      bool_t	dc, d_dc;		/* indicates whether DC is used */

      /*
       *  Compute minimum and maximum level of delta and normal approximations
       */
      min_level = d_min_level = MAXLEVEL;
      max_level = d_max_level = 0;
      dc 	= d_dc	   = NO;
   
      for (state = wfa->basis_states; state < wfa->states; state++)
	 for (label = 0; label < MAXLABELS; label++)
	    if (isrange (wfa->tree [state][label]))
	    {
	       if (delta_approx && wfa->delta_state [state])
	       {
		  d_min_level = min (d_min_level,
				     wfa->level_of_state [state] - 1);
		  d_max_level = max (d_max_level,
				     wfa->level_of_state [state] - 1);
		  if (wfa->into [state][label][0] == 0)
		     d_dc = YES;
	       }
	       else
	       {
		  min_level = min (min_level, wfa->level_of_state [state] - 1);
		  max_level = max (max_level, wfa->level_of_state [state] - 1);
		  if (wfa->into [state][label][0] == 0)
		     dc = YES;
	       }
	    }
      if (min_level > max_level)		/* no lc found */
	 max_level = min_level - 1;
      if (d_min_level > d_max_level)
	 d_max_level = d_min_level - 1;

      offset1 = dc ? 1 : 0;
      offset2 = offset1 + (d_dc ? 1 : 0);
      offset3 = offset2 + (max_level - min_level + 1);
      offset4 = offset3 + (d_max_level - d_min_level + 1);

      lptr = level_array = fiasco_calloc (total, sizeof (int));
      for (state = wfa->basis_states; state < wfa->states; state++)
	 for (label = 0; label < MAXLABELS; label++)
	    if (isrange (wfa->tree[state][label]))
	       for (edge = 0; isedge (domain = wfa->into[state][label][edge]);
		    edge++)
	       {
		  if ((unsigned) (lptr - level_array) >= total)
		     error ("Can't read more than %d weights.", total);
		  if (domain)
		  {
		     if (delta_approx && wfa->delta_state [state])
			*lptr++ = offset3 + wfa->level_of_state [state]
				  - 1 - d_min_level;
		     else
			*lptr++ = offset2 + wfa->level_of_state [state]
				  - 1 - min_level;
		  }
		  else
		     *lptr++ = delta_approx && wfa->delta_state [state]
			       ? offset1 : 0;
	       }
   }

   /*
    *  Decode the list of weights with an arithmetic decoder
    */
   {
      unsigned	      i;
      unsigned	     *c_symbols = fiasco_calloc (offset4, sizeof (unsigned));
      const unsigned  scale 	= 500; 	/* scaling of probability model */

      c_symbols [0] = 1 << (wfa->wfainfo->dc_rpf->mantissa_bits + 1);
      if (offset1 != offset2)
	 c_symbols [offset1] = 1 << (wfa->wfainfo->d_dc_rpf->mantissa_bits
				     + 1);
      for (i = offset2; i < offset3; i++)
	 c_symbols [i] = 1 << (wfa->wfainfo->rpf->mantissa_bits + 1);
      for (; i < offset4; i++)
	 c_symbols [i] = 1 << (wfa->wfainfo->d_rpf->mantissa_bits + 1);
      
      weights_array = decode_array (input, level_array, c_symbols,
				    offset4, total, scale);
      fiasco_free (c_symbols);
   }
   fiasco_free (level_array);

   /*
    *  Update transitions with decoded weights
    */
   {
      unsigned *wptr = weights_array;	/* pointer to current weight */
      int	domain;			/* current domain */

      for (state = wfa->basis_states; state < wfa->states; state++)
	 for (label = 0; label < MAXLABELS; label++)
	    if (isrange (wfa->tree[state][label]))
	       for (edge = 0; isedge (domain = wfa->into[state][label][edge]);
		    edge++)
	       {
		  real_t weight_val;
		  
		  if (domain)		/* not DC component */
		  {
		     if (delta_approx && wfa->delta_state [state])
			weight_val = btor (*wptr++, wfa->wfainfo->d_rpf);
		     else
			weight_val = btor (*wptr++, wfa->wfainfo->rpf);
		  }
		  else
		  {
		     if (delta_approx && wfa->delta_state [state])
			weight_val = btor (*wptr++, wfa->wfainfo->d_dc_rpf);
		     else
			weight_val = btor (*wptr++, wfa->wfainfo->dc_rpf);
		  }
		  
		  wfa->weight [state][label][edge] = weight_val;
		  wfa->int_weight [state][label][edge]
		     = weight_val * 512 + 0.5;
		  
		  /* CHECKPOINT: Weight read */
		  CP_READ_WEIGHT_ORIG(state, label, edge, domain, weight_val);
	       }
   }
   
   fiasco_free (weights_array);
}
 
