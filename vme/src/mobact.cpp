/*
 $Author: tperry $
 $RCSfile: mobact.cpp,v $
 $Date: 2002/07/01 01:30:58 $
 $Revision: 2.5 $
 */

#include "mobact.h"

#include "comm.h"
#include "db.h"
#include "dilrun.h"
#include "handler.h"
#include "interpreter.h"
#include "main_functions.h"
#include "slog.h"
#include "spec_assign.h"
#include "structs.h"
#include "szonelog.h"
#include "utils.h"
#include "zone_reset.h"

void SetFptrTimer(unit_data *u, unit_fptr *fptr)
{
    ubit32 ticks = 0;

    assert(!u->is_destructed());
    assert(!fptr->is_destructed());

    if ((ticks = fptr->heart_beat) > 0)
    {
        if (ticks < PULSE_SEC)
        {
            szonelog(g_boot_zone, "Error: %s@%s had heartbeat of %d.", UNIT_FI_NAME(u), UNIT_FI_ZONENAME(u), ticks);
            if ((fptr->getFunctionPointerIndex() == SFUN_DILCOPY_INTERNAL) || (fptr->getFunctionPointerIndex() == SFUN_DIL_INTERNAL))
            {
                dilprg *p = nullptr;
                p = (dilprg *)fptr->data;

                if (p)
                {
                    szonelog(g_boot_zone, "DIL [%s] had heartbeat issue", p->fp->tmpl->prgname);
                }
            }
            ticks = fptr->heart_beat = PULSE_SEC * 3;
        }

        if (IS_SET(fptr->flags, SFB_RANTIME))
        {
            ticks = number(ticks - ticks / 2, ticks + ticks / 2);
        }
        if (fptr->event)
        {
            g_events.remove(special_event, u, fptr);
        }
        fptr->event = g_events.add(ticks, special_event, u, fptr);
        //      g_events.add(ticks, special_event, u, fptr);
        membug_verify_class(fptr);
        membug_verify(fptr->data);
    }
}

void ResetFptrTimer(unit_data *u, unit_fptr *fptr)
{
    membug_verify_class(u);
    membug_verify_class(fptr);
    membug_verify(fptr->data);

    g_events.remove(special_event, u, fptr);
    SetFptrTimer(u, fptr);

    membug_verify(fptr->data);
}

void special_event(void *p1, void *p2)
{
    unit_data *u = (unit_data *)p1;
    unit_fptr *fptr = (unit_fptr *)p2;
    int priority = 0;

    ubit32 ret = SFR_SHARE;
    unit_fptr *ftmp = nullptr;
    spec_arg sarg;

    /*    if (fptr->index == SFUN_DIL_INTERNAL)
            if (fptr && fptr->data)
                    if (strcmp(((class dilprg *) fptr->data)->fp->tmpl->prgname, "wander_zones")==0)
                    {
                        if (strcmp(UNIT_FI_ZONENAME(u), "gnome") == 0)
                            slog(LOG_ALL, 0, "The wanderer!");
                    } MS2020 debug */

    if (g_cServerConfig.isNoSpecials())
    {
        return;
    }

    if (!u)
    {
        return;
    }
    if (!fptr)
    {
        return;
    }
    if (u->is_destructed())
    {
        return;
    }
    if (fptr->is_destructed())
    {
        return;
    }
    if (fptr->event)
    {
        fptr->event->func = nullptr;
    }

    fptr->event = nullptr;
    priority = FALSE;

    for (ftmp = UNIT_FUNC(u); ftmp; ftmp = ftmp->next)
    {
        if (ftmp == fptr)
        {
            break;
        }

        if (IS_SET(ftmp->flags, SFB_PRIORITY))
        {
            priority = TRUE;
        }
    }

    if (!priority)
    /* If switched, disable all tick functions, so we can control the mother fucker!
           if (!IS_CHAR (u) || !CHAR_IS_SWITCHED (u)) */
    {
        if (g_unit_function_array[fptr->getFunctionPointerIndex()].func)
        {
            if (IS_SET(fptr->flags, SFB_TICK))
            {
#ifdef DEBUG_HISTORY
                add_func_history(u, fptr->index, SFB_TICK);
#endif
                sarg.owner = u;
                sarg.activator = nullptr;
                sarg.fptr = fptr;
                sarg.cmd = &g_cmd_auto_tick;
                sarg.arg = "";
                sarg.mflags = SFB_TICK;
                sarg.medium = nullptr;
                sarg.target = nullptr;
                sarg.pInt = nullptr;

                ret = (*(g_unit_function_array[fptr->getFunctionPointerIndex()].func))(&sarg);
            }
            assert((ret == SFR_SHARE) || (ret == SFR_BLOCK));
        }
        else
        {
            slog(LOG_ALL, 0, "Null function call!");
        }
    }
    else // Not executed because SFUN Before it raised priority
    {
        if (fptr->getFunctionPointerIndex() == SFUN_DIL_INTERNAL)
        {
            if (fptr->heart_beat == 1)
            {
                fptr->heart_beat = 5 * PULSE_SEC;
            }
        }
    }

    if (fptr->is_destructed())
    {
        return;
    }

    if (fptr->heart_beat < PULSE_SEC)
    {
        slog(LOG_ALL, 0, "Error: %s@%s had heartbeat of %d.", UNIT_FI_NAME(u), UNIT_FI_ZONENAME(u), fptr->heart_beat);
    }

    if (fptr->getFunctionPointerIndex() == SFUN_DIL_INTERNAL)
    {
        int diltick = 0;
        int i = 0;
        diltick = FALSE;
        if (IS_SET(fptr->flags, SFB_TICK))
        {
            diltick = TRUE;
        }
        else if (fptr->data)
        {
            dilprg *prg = (dilprg *)fptr->data;
            for (i = 0; i < prg->fp->intrcount; i++)
            {
                if IS_SET (prg->fp->intr[i].flags, SFB_TICK)
                    diltick = TRUE;
            }
        }
        if (!diltick)
        {
            return;
        }
    }

    if (!u->is_destructed() && !fptr->is_destructed())
    {
        SetFptrTimer(u, fptr);
    }
}

/* Return TRUE while stopping events */
void stop_special(unit_data *u, unit_fptr *fptr)
{
    g_events.remove(special_event, u, fptr);
}

void start_special(unit_data *u, unit_fptr *fptr)
{
    int diltick = 0;
    int i = 0;
    if (fptr->getFunctionPointerIndex() == SFUN_DIL_INTERNAL)
    {
        if (IS_SET(fptr->flags, SFB_TICK))
        {
            diltick = 1;
        }
        else if (fptr->data)
        {
            dilprg *prg = (dilprg *)fptr->data;
            for (i = 0; i < prg->fp->intrcount; i++)
            {
                if (IS_SET(prg->fp->intr[i].flags, SFB_TICK))
                {
                    diltick = 1;
                }
            }
        }
        if (!diltick)
        {
            return;
        }
    }

    if (IS_SET(fptr->flags, SFB_TICK) || fptr->getFunctionPointerIndex() == SFUN_DIL_INTERNAL)
    {
        /* If people forget to set the ticking functions... */
        if (fptr->heart_beat <= 0)
        {
            fptr->heart_beat = g_unit_function_array[fptr->getFunctionPointerIndex()].tick;

            /* Well, the builders are supposed to fix it! That's why it is
               sent to the log, so they can see it! */
            /* HUUUGE amount of log :(  /gnort */
            /* Now we default for the suckers... no need to warn any more
               szonelog(g_boot_zone,
               "%s@%s(%s): Heartbeat was 0 (idx %d), "
               "set to defualt: %d",
               UNIT_FI_NAME(u), UNIT_FI_ZONENAME(u),
               UNIT_NAME(u), fptr->index,
               g_unit_function_array[fptr->index].tick); */
        }

        //      g_events.add(fptr->heart_beat, special_event, u, fptr);
        if (fptr->event)
        {
            g_events.remove(special_event, u, fptr);
        }

        if (!u->is_destructed() && !fptr->is_destructed())
        {
            fptr->event = g_events.add(fptr->heart_beat, special_event, u, fptr);
        }
    }
}

void start_all_special(unit_data *u)
{
    unit_fptr *fptr = nullptr;

    for (fptr = UNIT_FUNC(u); fptr; fptr = fptr->next)
    {
        start_special(u, fptr);
    }
}

void stop_all_special(unit_data *u)
{
    unit_fptr *fptr = nullptr;

    for (fptr = UNIT_FUNC(u); fptr; fptr = fptr->next)
    {
        stop_special(u, fptr);
    }
}
