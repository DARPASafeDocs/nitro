/* =========================================================================
 * This file is part of NITRO
 * =========================================================================
 * 
 * (C) Copyright 2004 - 2008, General Dynamics - Advanced Information Systems
 *
 * NITRO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, If not, 
 * see <http://www.gnu.org/licenses/>.
 *
 */

#include "nitf/TRECursor.h"
#include "nitf/TREPrivateData.h"


NITFAPI(nitf_TRECursor) nitf_TRECursor_begin(nitf_TRE * tre)
{
    nitf_Error error;
    nitf_TRECursor tre_cursor;
    nitf_TREDescription *dptr;
	
    tre_cursor.loop = nitf_IntStack_construct(&error);
    tre_cursor.loop_idx = nitf_IntStack_construct(&error);
    tre_cursor.loop_rtn = nitf_IntStack_construct(&error);
    tre_cursor.numItems = 0;
    tre_cursor.index = 0;
    tre_cursor.looping = 0;

    if (tre)
    {
        /* set the start index */
        tre_cursor.index = -1;
        /* count how many descriptions there are */
		dptr = ((nitf_TREPrivateData*)tre->priv)->description;
		
        while (dptr && (dptr->data_type != NITF_END))
        {
            tre_cursor.numItems++;
            dptr++;
        }
        memset(tre_cursor.tag_str, 0, 256);
		sprintf(tre_cursor.tag_str, "%s",
		        ((nitf_TREPrivateData*)tre->priv)->description->tag);
        tre_cursor.tre = tre;
    }

    return tre_cursor;
}


/**
 * Normalizes the tag, which could be in a loop, and returns the nitf_Pair* from
 * the TRE hash that corresponds to the current normalized tag.
 */
NITFAPI(nitf_Pair *) nitf_TRECursor_getTREPair(nitf_TRE * tre,
                                 nitf_TREDescription * desc_ptr,
                                 char idx_str[10][10],
                                 int looping, nitf_Error * error)
{
    char tag_str[256];          /* temp buf used for storing the qualified tag */
    char *bracePtr;             /* pointer for brace position */
    int index = 0;              /* used as in iterator */
    int i;                      /* temp used for looping */
    nitf_Pair *pair = NULL;     /* the pair to return */

    strncpy(tag_str, desc_ptr->tag, sizeof(tag_str));
    /* deal with braces */
    if (strchr(desc_ptr->tag, '['))
    {
        index = 0;
        bracePtr = desc_ptr->tag;
        /* split the string */
        *(strchr(tag_str, '[')) = 0;
        bracePtr--;
        while ((bracePtr = strchr(bracePtr + 1, '[')) != NULL)
        {
            /* tack on the depth */
            strcat(tag_str, idx_str[index++]);
        }
    }
    else
    {
        /* it is dependent on something in another loop,
         * so, we need to figure out what level.
         * since tags are unique, we are ok checking like this */
        pair = nitf_HashTable_find(tre->hash, tag_str);
        for (i = 0; i < looping && !pair; ++i)
        {
            strcat(tag_str, idx_str[i]);
            pair = nitf_HashTable_find(tre->hash, tag_str);
        }
    }
    /* pull the data from the hash about the dependent loop value */
    pair = nitf_HashTable_find(tre->hash, tag_str);
    return pair;
}



NITFAPI(void) nitf_TRECursor_cleanup(nitf_TRECursor * tre_cursor)
{
    nitf_IntStack_destruct(&tre_cursor->loop);
    nitf_IntStack_destruct(&tre_cursor->loop_idx);
    nitf_IntStack_destruct(&tre_cursor->loop_rtn);
}


NITFAPI(NITF_BOOL) nitf_TRECursor_isDone(nitf_TRECursor * tre_cursor)
{
    int status = 1;
    int i = 0;
    int gotField = 0;
    nitf_Error error;
    NITF_BOOL iterStatus = NITF_SUCCESS;
    nitf_TRECursor cursor = nitf_TRECursor_begin(tre_cursor->tre);

    /* check if the passed in cursor just began */
    if (tre_cursor->index < 0)
        status = 0;

    /* first check all loops to see if we are in the middle of one */
    for (i = 0; status && (i < tre_cursor->looping); ++i)
    {
        if (tre_cursor->loop_idx->st[i] < tre_cursor->loop->st[i])
        {
            status = 0;
        }
    }

    /* try iterating and see if we make it to the end */
    while (status && (cursor.index < cursor.numItems) && iterStatus == NITF_SUCCESS)
    {
        iterStatus = nitf_TRECursor_iterate(&cursor, &error);

        if (gotField)
        {
            /* we got to the next field... so return not done */
            status = 0;
        }
        else if (strcmp(tre_cursor->tag_str, cursor.tag_str) == 0)
        {
            gotField = 1;
        }
    }

    /* and the current status with a check to see if we reached the end */
    status = (status & (cursor.index >= cursor.numItems)) | !iterStatus;
    nitf_TRECursor_cleanup(&cursor);
    return status;
}

NITFAPI(int) nitf_TRECursor_iterate(nitf_TRECursor * tre_cursor,
                              nitf_Error * error)
{
    nitf_TREDescription *dptr;

    int *stack;                 /* used for in conjuction with the stacks */
    int index;                  /* used for in conjuction with the stacks */

    int loopCount = 0;          /* tells how many times to loop */
    int loop_rtni = 0;          /* used for temp storage */
    int loop_idxi = 0;          /* used for temp storage */

    int numIfs = 0;             /* used to keep track of nested Ifs */
    int numLoops = 0;           /* used to keep track of nested Loops */
    int done = 0;               /* flag used for special cases */

    char idx_str[10][10];       /* used for keeping track of indexes */
	
    if (!tre_cursor->loop || !tre_cursor->loop_idx
            || !tre_cursor->loop_rtn)
    {
        /* not initialized */
        nitf_Error_init(error, "Unhandled TRE Value data type",
                        NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return NITF_FAILURE;
    }

    /* count how many descriptions there are */
    
	dptr = ((nitf_TREPrivateData*)tre_cursor->tre->priv)->description;

    while (!done)
    {
        done = 1;               /* set the flag */

        /* iterate the index */
        tre_cursor->index++;

        if (tre_cursor->index < tre_cursor->numItems)
        {
            memset(tre_cursor->tag_str, 0, 256);

            tre_cursor->prev_ptr = tre_cursor->desc_ptr;
            tre_cursor->desc_ptr = &dptr[tre_cursor->index];

            /* if already in a loop, prepare the array of values */
            if (tre_cursor->looping)
            {
                stack = tre_cursor->loop_idx->st;
                /* assert, because we only prepare for 10 */
                assert(tre_cursor->looping <= 10);

                for (index = 0; index < tre_cursor->looping; index++)
                {
                    sprintf(idx_str[index], "[%d]", stack[index]);
                }
            }

            /* check if it is an actual item now */
            /* ASCII string */
            if ((tre_cursor->desc_ptr->data_type == NITF_BCS_A) ||
                    /* ASCII number */
                    (tre_cursor->desc_ptr->data_type == NITF_BCS_N) ||
                    /* raw bytes */
                    (tre_cursor->desc_ptr->data_type == NITF_BINARY))
            {
                sprintf(tre_cursor->tag_str, "%s",
                        tre_cursor->desc_ptr->tag);
                /* check if data is part of an array */
                if (tre_cursor->looping)
                {
                    stack = tre_cursor->loop_idx->st;
                    for (index = 0; index < tre_cursor->looping; index++)
                    {
                        char entry[64];
                        sprintf(entry, "[%d]", stack[index]);
                        strcat(tre_cursor->tag_str, entry);
                    }
                }
                /* check to see if we don't know the length */
                if (tre_cursor->desc_ptr->data_count ==
                        NITF_TRE_CONDITIONAL_LENGTH)
                {
                    /* compute the length given by the previous field */
                    tre_cursor->length =
                        nitf_TRECursor_evalCondLength(tre_cursor->tre,
                                                   tre_cursor->prev_ptr,
                                                   idx_str,
                                                   tre_cursor->looping,
                                                   error);
                    /* This check was added because of this scenario:
                     * What if the value that the conditional length is based upon
                     * happens to be zero??? That means that technically the field
                     * should not exist. Since we enforce fields to be of size > 0,
                     * then we will just take care of this scenario now, and just
                     * iterate the cursor.
                     */
                    if (tre_cursor->length == 0)
                    {
                        return nitf_TRECursor_iterate(tre_cursor, error);
                    }
                }
                else
                {
                    /* just set the length that was in the TREDescription */
                    tre_cursor->length = tre_cursor->desc_ptr->data_count;
                }
            }
            /* NITF_LOOP, NITF_IF, etc. */
            else if ((tre_cursor->desc_ptr->data_type >=
                      NITF_LOOP)
                     && (tre_cursor->desc_ptr->data_type < NITF_END))
            {
                done = 0;       /* set the flag */

                /* start of a loop */
                if (tre_cursor->desc_ptr->data_type == NITF_LOOP)
                {
                    loopCount =
                        nitf_TRECursor_evalLoops(tre_cursor->tre,
                                       tre_cursor->desc_ptr, idx_str,
                                       tre_cursor->looping, error);
                    if (loopCount > 0)
                    {
                        tre_cursor->looping++;
                        /* record loopcount in @loop stack */
                        nitf_IntStack_push(tre_cursor->loop, loopCount,
                                           error);
                        /* record i in @loop_rtn stack */
                        nitf_IntStack_push(tre_cursor->loop_rtn,
                                           tre_cursor->index, error);
                        /* record a 0 in @loop_idx stack */
                        nitf_IntStack_push(tre_cursor->loop_idx, 0, error);
                    }
                    else
                    {
                        numLoops = 1;
                        /* skip until we see the matching ENDLOOP */
                        while (numLoops
                                && (++tre_cursor->index <
                                    tre_cursor->numItems))
                        {
                            tre_cursor->desc_ptr =
                                &dptr[tre_cursor->index];
                            if (tre_cursor->desc_ptr->data_type ==
                                    NITF_LOOP)
                                numLoops++;
                            else if (tre_cursor->desc_ptr->data_type ==
                                     NITF_ENDLOOP)
                                numLoops--;
                        }
                    }
                }
                /* end of a loop */
                else if (tre_cursor->desc_ptr->data_type == NITF_ENDLOOP)
                {
                    /* retrieve loopcount from @loop stack */
                    loopCount = nitf_IntStack_pop(tre_cursor->loop, error);
                    /* retrieve loop_rtn from @loop_rtn stack */
                    loop_rtni =
                        nitf_IntStack_pop(tre_cursor->loop_rtn, error);
                    /* retrieve loop_idx from @loop_idx stack */
                    loop_idxi =
                        nitf_IntStack_pop(tre_cursor->loop_idx, error);

                    if (--loopCount > 0)
                    {
                        /* record loopcount in @loop stack */
                        nitf_IntStack_push(tre_cursor->loop, loopCount,
                                           error);
                        /* record i in @loop_rtn stack */
                        nitf_IntStack_push(tre_cursor->loop_rtn, loop_rtni,
                                           error);
                        /* record idx in @loop_idx stack */
                        nitf_IntStack_push(tre_cursor->loop_idx,
                                           ++loop_idxi, error);
                        /* jump to the start of the loop */
                        tre_cursor->index = loop_rtni;
                    }
                    else
                    {
                        --tre_cursor->looping;
                    }
                }
                /* an if clause */
                else if (tre_cursor->desc_ptr->data_type == NITF_IF)
                {
                    if (!nitf_TRECursor_evalIf
                            (tre_cursor->tre, tre_cursor->desc_ptr, idx_str,
                             tre_cursor->looping, error))
                    {
                        numIfs = 1;
                        /* skip until we see the matching ENDIF */
                        while (numIfs
                                && (++tre_cursor->index <
                                    tre_cursor->numItems))
                        {
                            tre_cursor->desc_ptr =
                                &dptr[tre_cursor->index];
                            if (tre_cursor->desc_ptr->data_type == NITF_IF)
                                numIfs++;
                            else if (tre_cursor->desc_ptr->data_type ==
                                     NITF_ENDIF)
                                numIfs--;
                        }
                    }
                }
                /* compute the length */
                else if (tre_cursor->desc_ptr->data_type == NITF_COMP_LEN)
                {
                    /*do nothing */
                }
            }
            else
            {
                nitf_Error_init(error, "Unhandled TRE Value data type",
                                NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
                return NITF_FAILURE;
            }
        }
        else
        {
            /* should return FALSE instead. signifies we are DONE iterating! */
            return NITF_FAILURE;
        }
    }
    return NITF_SUCCESS;
}


/**
 * Helper function for evaluating loops
 * Returns the number of loops that will be processed
 */
NITFAPI(int) nitf_TRECursor_evalLoops(nitf_TRE * tre,
                             nitf_TREDescription * desc_ptr,
                             char idx_str[10][10],
                             int looping, nitf_Error * error)
{
    int loops;
    char str[256];              /* temp buf used for manipulating the loop label */
    nitf_Pair *pair;            /* temp nitf_Pair */
    nitf_Field *field;          /* temp nitf_Field */

    char *op;
    char *valPtr;
    int loopVal;                /* used for the possible data in the loop label */

    /* if the user wants a constant value */
    if (desc_ptr->label && strcmp(desc_ptr->label, NITF_CONST_N) == 0)
    {
        loops = NITF_ATO32(desc_ptr->tag);
    }

    else if (desc_ptr->label && strcmp(desc_ptr->label, NITF_FUNCTION) == 0)
    {
	int i;
	NITF_TRE_CURSOR_COUNT_FUNCTION fn = 
	    (NITF_TRE_CURSOR_COUNT_FUNCTION*)desc_ptr->tag;


	loops = (*fn)(tre, idx_str, looping, error);

	if (loops == -1)
	    return NITF_FAILURE;
    }

    else
    {
        pair = nitf_TRECursor_getTREPair(tre, desc_ptr, idx_str, looping, error);
        if (!pair)
        {
            nitf_Error_init(error,
                            "nitf_TRECursor_evalLoops: invalid TRE loop counter",
                            NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
            return NITF_FAILURE;
        }
        field = (nitf_Field *) pair->data;

        /* get the int value */
        if (!nitf_Field_get
                (field, (char *) &loops, NITF_CONV_INT, sizeof(loops), error))
        {
            return NITF_FAILURE;
        }

        /* if the label is not empty, then apply some functionality */
        if (desc_ptr->label && strlen(desc_ptr->label) != 0)
        {
            assert(strlen(desc_ptr->label) < sizeof(str));

            strcpy(str, desc_ptr->label);
            op = str;
            while (isspace(*op))
                op++;
            if ((*op == '+') ||
                    (*op == '-') ||
                    (*op == '*') || (*op == '/') || (*op == '%'))
            {
                valPtr = op + 1;
                while (isspace(*valPtr))
                    valPtr++;

                loopVal = NITF_ATO32(valPtr);

                switch (*op)
                {
                    case '+':
                        loops += loopVal;
                        break;
                    case '-':
                        loops -= loopVal;
                        break;
                    case '*':
                        loops *= loopVal;
                        break;
                    case '/':
                        /* check for divide by zero */
                        if (loopVal == 0)
                        {
                            nitf_Error_init(error,
                                            "nitf_TRECursor_evalLoops: attempt to divide by zero",
                                            NITF_CTXT,
                                            NITF_ERR_INVALID_PARAMETER);
                            return NITF_FAILURE;
                        }
                        loops /= loopVal;
                        break;
                    case '%':
                        loops %= loopVal;
                        break;
                    default:
                        break;
                }
            }
            else
            {
                nitf_Error_init(error, "nitf_TRECursor_evalLoops: invalid operator",
                                NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
                return NITF_FAILURE;
            }
        }
    }
    return loops < 0 ? 0 : loops;
}


NITFAPI(int) nitf_TRECursor_evalIf(nitf_TRE * tre,
                          nitf_TREDescription * desc_ptr,
                          char idx_str[10][10],
                          int looping, nitf_Error * error)
{
    nitf_Field *field;
    nitf_Pair *pair;

    char str[256];              /* temp buf for the label */
    char *emptyPtr;
    char *op;
    char *valPtr;

    int status = 0;             /* the return status */
    int fieldData;              /* used as the value for comparing */
    int treData;                /* the value defined int the TRE descrip */

    /* get the data out of the hashtable */
    pair = nitf_TRECursor_getTREPair(tre, desc_ptr, idx_str, looping, error);
    if (!pair)
    {
        nitf_Error_init(error, "Unable to find tag in TRE hash",
                        NITF_CTXT, NITF_ERR_UNK);
        return NITF_FAILURE;
    }
    field = (nitf_Field *) pair->data;
    assert(strlen(desc_ptr->label) < sizeof(str));

    strcpy(str, desc_ptr->label);
    op = str;
    while (isspace(*op))
        op++;

    /* split the string at the space */
    emptyPtr = strchr(op, ' ');
    *emptyPtr = 0;

    /* remember where the operand is */
    valPtr = emptyPtr + 1;

    /* check if it is a string comparison of either 'eq' or 'ne' */
    if ((strcmp(op, "eq") == 0) || (strcmp(op, "ne") == 0))
    {
        /* must be a string */
        if (field->type == NITF_BCS_N)
        {
            nitf_Error_init(error,
                            "evaluate: can't use eq/ne to compare a number",
                            NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
            return NITF_FAILURE;
        }
        status = strncmp(field->raw, valPtr, field->length);
        status = strcmp(op, "eq") == 0 ? !status : status;
    }
    /* check if it is a logical operator for ints */
    else if ((strcmp(op, "<") == 0) ||
             (strcmp(op, ">") == 0) ||
             (strcmp(op, ">=") == 0) ||
             (strcmp(op, "<=") == 0) ||
             (strcmp(op, "==") == 0) || (strcmp(op, "!=") == 0))
    {
        /* make sure it is a number */
        if (field->type != NITF_BCS_N)
        {
            nitf_Error_init(error,
                            "evaluate: can't use strings for logical expressions",
                            NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
            return NITF_FAILURE;
        }

        treData = NITF_ATO32(valPtr);
        if (!nitf_Field_get
                (field, (char *) &fieldData, NITF_CONV_INT, sizeof(fieldData),
                 error))
        {
            return NITF_FAILURE;
        }

        /* 0 -> equal, <0 -> less true, >0 greater true */
        status = fieldData - treData;

        if (strcmp(op, ">") == 0)
            status = (status > 0);
        else if (strcmp(op, "<") == 0)
            status = (status < 0);
        else if (strcmp(op, ">=") == 0)
            status = (status >= 0);
        else if (strcmp(op, "<=") == 0)
            status = (status <= 0);
        else if (strcmp(op, "==") == 0)
            status = (status == 0);
        else if (strcmp(op, "!=") == 0)
            status = (status != 0);
    }
    /* otherwise, they used a bad operator */
    else
    {
        nitf_Error_init(error, "evaluate: invalid comparison operator",
                        NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return NITF_FAILURE;
    }
    return status;
}



/**
 * Helper function for evaluating loops
 * Returns the number of loops that will be processed
 */
NITFAPI(int) nitf_TRECursor_evalCondLength(nitf_TRE * tre,
        nitf_TREDescription * desc_ptr,
        char idx_str[10][10],
        int looping, nitf_Error * error)
{
    int computedLength;
    char str[256];              /* temp buf used for manipulating the loop label */
    nitf_Pair *pair;            /* temp nitf_Pair */
    nitf_Field *field;          /* temp nitf_Field */

    char *op;
    char *valPtr;
    int funcVal;                /* used for the possible data in the description label */

    pair = nitf_TRECursor_getTREPair(tre, desc_ptr, idx_str, looping, error);
    if (!pair)
    {
        nitf_Error_init(error,
                        "nitf_TRECursor_evalCondLength: invalid TRE reference",
                        NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return NITF_FAILURE;
    }
    field = (nitf_Field *) pair->data;

    /* get the int value */
    if (!nitf_Field_get
            (field, (char *) &computedLength, NITF_CONV_INT,
             sizeof(computedLength), error))
    {
        return NITF_FAILURE;
    }

    /* if the label is not empty, then apply some functionality */
    if (desc_ptr->label && strlen(desc_ptr->label) != 0)
    {
        assert(strlen(desc_ptr->label) < sizeof(str));

        strcpy(str, desc_ptr->label);
        op = str;
        while (isspace(*op))
            op++;
        if ((*op == '+') ||
                (*op == '-') || (*op == '*') || (*op == '/') || (*op == '%'))
        {
            valPtr = op + 1;
            while (isspace(*valPtr))
                valPtr++;

            funcVal = NITF_ATO32(valPtr);

            switch (*op)
            {
                case '+':
                    computedLength += funcVal;
                    break;
                case '-':
                    computedLength -= funcVal;
                    break;
                case '*':
                    computedLength *= funcVal;
                    break;
                case '/':
                    /* check for divide by zero */
                    if (funcVal == 0)
                    {
                        nitf_Error_init(error,
                                        "nitf_TRECursor_evalCondLength: attempt to divide by zero",
                                        NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
                        return NITF_FAILURE;
                    }
                    computedLength /= funcVal;
                    break;
                case '%':
                    computedLength %= funcVal;
                    break;
                default:
                    break;
            }
        }
        else
        {
            nitf_Error_init(error,
                            "nitf_TRECursor_evalCondLength: invalid operator",
                            NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
            return NITF_FAILURE;
        }
    }
    return computedLength < 0 ? 0 : computedLength;
}
