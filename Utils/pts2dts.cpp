#include "pts2dts.hpp"

int cmpvlctickp(const void *p1, const void *p2)
{
    if (*((mtime_t *)p1) >= *((mtime_t *)p2))
        return *((mtime_t *)p1) > *((mtime_t *)p2) ? 1 : 0;
    else
        return -1;
}

void dtsgen_Init(struct dtsgen_t *d)
{
    d->count = 0;
    d->reorderdepth = 0;
}

void dtsgen_Resync(struct dtsgen_t *d)
{
    d->count = 0;
    d->reorderdepth = 0;
}

void dtsgen_AddNextPTS(struct dtsgen_t *d, mtime_t i_pts)
{
    /* Check saved pts in reception order to find reordering depth */
    if (d->count > 0 && d->count < DTSGEN_HISTORY_COUNT)
    {
        unsigned i;
        if (d->count > (1 + d->reorderdepth))
            i = d->count - (1 + d->reorderdepth);
        else
            i = 0;

        for (; i < d->count; i++)
        {
            if (d->history[i] > i_pts)
            {
                if (d->reorderdepth < DTSGEN_REORDER_MAX)
                    d->reorderdepth++;
            }
            break;
        }
    }

    /* insert current */
    if (d->count == DTSGEN_HISTORY_COUNT)
    {
        d->ordereddts[0] = i_pts; /* remove lowest */
        memmove(d->history, &d->history[1], sizeof(d->history[0]) * (d->count - 1));
    }
    else
    {
        d->history[d->count] = i_pts;
        d->ordereddts[d->count++] = i_pts;
    }

    /* order pts in second list, will be used as dts */
    qsort(&d->ordereddts, d->count, sizeof(d->ordereddts[0]), cmpvlctickp);
}

mtime_t dtsgen_GetDTS(struct dtsgen_t *d)
{
    mtime_t i_dts = VLC_TS_INVALID;

    /* When we have inspected enough packets,
     * use the reorderdepth th packet as dts offset */
    if (d->count > DTSGEN_REORDER_MAX)
    {
        i_dts = d->ordereddts[d->count - d->reorderdepth - 1];
    }
    /* When starting, we craft a slow incrementing DTS to ensure
       we can't go backward due to reorder need */
    else if (d->count == 1)
    {
        d->i_startingdts = i_dts = __MAX(d->history[0] - 150000, VLC_TS_0);
        d->i_startingdiff = d->history[0] - i_dts;
    }
    else if (d->count > 1)
    {
        mtime_t i_diff = d->ordereddts[d->count - 1] - d->ordereddts[0];
        i_diff = __MIN(d->i_startingdiff, i_diff);
        d->i_startingdts += i_diff / DTSGEN_REORDER_MAX;
        i_dts = d->i_startingdts;
    }

    return i_dts;
}
