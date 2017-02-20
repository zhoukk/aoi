/**
 * author: zhoukk
 * link: https://github.com/zhoukk/aoi
 *
 * Area of Interest
 *
 * LD_FLAGS += -lm
 *
 * example:
 *
 * #define AOI_IMPLEMENTATION
 * #include "aoi.h"
 * #include <time.h>
 * #include <unistd.h>
 * static int
 * enter(struct aoi *aoi) {
 *     int id, x, y, speed;
 *     id = aoi_enter(aoi, 0);
 *     speed = rand()%10+4;
 *     x = rand()%100 + 100;
 *     y = rand()%100 + 100;
 *     aoi_speed(aoi, id, speed);
 *     aoi_locate(aoi, id, x, y);
 *     return id;
 * }
 * int
 * main(int argc, char *argv[]) {
 *     srand(time(0));
 *     struct aoi *aoi = (struct aoi *)malloc(aoi_memsize());
 *     aoi_init(aoi);
 *     int enter_r = 100;
 *     int leave_r = 130;
 *     int width = 1000;
 *     int height = 600;
 *     int step = 1000000;
 *     int id[3], i;
 *     id[0] = enter(aoi);
 *     id[1] = enter(aoi);
 *     id[2] = enter(aoi);
 *     printf("aoi enter radius :%d  leave radius :%d\n", enter_r, leave_r);
 *     while (1) {
 *         int x, y;
 *         for (i = 0; i < 3; i++) {
 *             if (!aoi_moving(aoi, id[i])) {
 *                 x = rand() % width;
 *                 y = rand() % height;
 *                 aoi_move(aoi, id[i], x, y);
 *             } else {
 *                 aoi_update(aoi, id[i], 1);
 *                 struct aoi_event *list;
 *                 int r = aoi_trigger(aoi, id[i], enter_r, leave_r, &list);
 *                 int j;
 *                 for (j = 0; j < r; j++) {
 *                    int sx, sy, tx, ty;
 *                    aoi_pos(aoi, id[i], &sx, &sy);
 *                    aoi_pos(aoi, list[j].id, &tx, &ty);
 *                    int dist = (int)sqrtf((sx-tx)*(sx-tx) + (sy-ty)*(sy-ty));
 *                    char *e;
 *                    if (list[j].e == AOI_ENTER) {
 *                        e = "enter";
 *                    } else {
 *                        e = "leave";
 *                    }
 *                    printf("[id: %d (%d,%d)] --> [id: %d (%d,%d)] %s dist:%d\n", id[i], sx, sy, list[j].id, tx, ty, e, dist);
 *                }
 *             }
 *         }
 *         usleep(step);
 *     }
 *     aoi_unit(aoi);
 *     free(aoi);
 *     return 0;
 * }
 *
 */


#ifndef _aoi_h_
#define _aoi_h_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AOI_API
#define AOI_API extern
#endif // AOI_API

/** Maximum objects allowed in aoi. */
#define AOI_MAX_AOI_P 16
#define AOI_MAX_AOI (1<<AOI_MAX_AOI_P)

/** Default aoi list size. */
#define AOI_DEF_AOI 32

/** Value for event used in trigger. */
#define AOI_ENTER 0x01      /** Some object into sight */
#define AOI_LEAVE 0x02      /** Some object out sight */

struct aoi;

struct aoi_event {
    int id;     /** Trigger id, who enter or leave sight */
    int e;      /** Trigger event, AOI_ENTER or AOI_LEAVE */
};

/** Memory size of struct aoi. */
AOI_API int aoi_memsize(void);

/** Initialize aoi. */
AOI_API void aoi_init(struct aoi *aoi);

/** Clear aoi. */
AOI_API void aoi_unit(struct aoi *aoi);

/** New object enter, allocate a id. */
AOI_API int aoi_enter(struct aoi *aoi, void *ud);

/** Object leave, recovery the id. */
AOI_API void aoi_leave(struct aoi *aoi, int id);

/** Get userdata of object. */
AOI_API void *aoi_ud(struct aoi *aoi, int id);

/** Locate the object to same place. */
AOI_API void aoi_locate(struct aoi *aoi, int id, int x, int y);

/** Start move the object to same place. */
AOI_API void aoi_move(struct aoi *aoi, int id, int x, int y);

/** Set the object speed. */
AOI_API void aoi_speed(struct aoi *aoi, int id, int speed);

/** Update the object moving status. */
AOI_API void aoi_update(struct aoi *aoi, int id, int tick);

/** Get current position of the object. */
AOI_API void aoi_pos(struct aoi *aoi, int id, int *px, int *py);

/**
 * Trigger aoi event of the object.
 * enter_r: maximum distance other object in sight
 * leave_r: minimum distance other object out sight
 * list: objects trigger enter or leave
 * make sure leave_r > enter_r
 */
AOI_API int aoi_trigger(struct aoi *aoi, int id, int enter_r, int leave_r,
                        struct aoi_event **list);

/** Whether the object is moving. */
AOI_API int aoi_moving(struct aoi *aoi, int id);

/** Get around objects in sight. */
AOI_API int aoi_around(struct aoi *aoi, int id, int *list, int n);

#ifdef __cplusplus
}
#endif

#endif // _aoi_h_


#ifdef AOI_IMPLEMENTATION

/**
 * Implement
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef min
#define min(x,y) ({\
        typeof(x) _x = (x);\
        typeof(y) _y = (y);\
        (void)(&_x == &y);\
        _x < _y ? _x : _y;})
#endif

#ifndef max
#define max(x,y) ({\
        typeof(x) _x = (x);\
        typeof(y) _y = (y);\
        (void)(&_x == &y);\
        _x > _y ? _x : _y;})
#endif


#define AOI_OBJECT_INVALID 0
#define AOI_OBJECT_RESERVE 1

#define AOI_HASH_ID(id) (id%AOI_MAX_AOI)

struct aoi_object {
    int id;
    int p[2];       /* cur pos in moving */
    int sp[2];      /* pos when start move */
    int dp[2];      /* move destination */
    float d[2];
    float e;
    int p_tick;     /* tick after move start */
    int n_tick;     /* tick before move end */
    int type;       /* invalid or revert */
    int speed;      /* object moving speed */
    struct aoi_object *prev[2];
    struct aoi_object *next[2];
    int *n_list;    /* new version object list around */
    int *o_list;    /* old version object list around */

    void *ud;   /* user data */
};

struct aoi {
    int id;
    struct aoi_object slot[AOI_MAX_AOI];    /* all object solt */
    struct aoi_object *list[2];             /* object list in x and y axis */
    struct aoi_event elist[AOI_MAX_AOI];	/* event list hold */
};


AOI_API int
aoi_memsize(void) {
    return sizeof(struct aoi);
}

AOI_API void
aoi_init(struct aoi *aoi) {
    memset(aoi, 0, sizeof *aoi);
    aoi->id = 0;
}

AOI_API void
aoi_unit(struct aoi *aoi) {

}

/**
 * Allocate id
 */
static int
_aoi_next_id(struct aoi *aoi) {
    int i;
    for (i = 0; i < AOI_MAX_AOI; i++) {
        struct aoi_object *obj;
        int id = aoi->id++;
        if (id < 0) {
            id = aoi->id + 0x7fffffff;
        }
        obj = &aoi->slot[AOI_HASH_ID(id)];
        if (obj->type == AOI_OBJECT_INVALID) {
            memset(obj, 0, sizeof *obj);
            obj->type = AOI_OBJECT_RESERVE;
            obj->id = id;
            return id;
        }
    }
    return -1;
}

/**
 * Get object from id
 */
static inline struct aoi_object *
_aoi_object(struct aoi *aoi, int id) {
    if (id < 0) return 0;
    struct aoi_object *obj = &aoi->slot[AOI_HASH_ID(id)];
    if (obj->id != id || obj->type == AOI_OBJECT_INVALID) {
        return 0;
    }
    return obj;
}

AOI_API int
aoi_enter(struct aoi *aoi, void *ud) {
    int id, i;
    struct aoi_object *obj;

    id = _aoi_next_id(aoi);
    if (-1 == id) {
        return -1;
    }
    obj = _aoi_object(aoi, id);
    if (!obj) {
        return -1;
    }
    for (i = 0; i < 2; i++) {
        obj->next[i] = aoi->list[i];
        if (aoi->list[i]) {
            aoi->list[i]->prev[i] = obj;
        }
        aoi->list[i] = obj;
    }
    obj->n_list = (int *)malloc((AOI_DEF_AOI + 2)*sizeof(int));
    obj->n_list[0] = 0;
    obj->n_list[1] = AOI_DEF_AOI;
    obj->o_list = (int *)malloc((AOI_DEF_AOI + 2)*sizeof(int));
    obj->o_list[0] = 0;
    obj->o_list[1] = AOI_DEF_AOI;
    obj->ud = ud;
    return id;
}

static inline void
_aoi_list_erase(struct aoi *aoi, int list, struct aoi_object *obj) {
    if (obj->prev[list]) {
        obj->prev[list]->next[list] = obj->next[list];
    } else {
        aoi->list[list] = obj->next[list];
    }
    if (obj->next[list]) {
        obj->next[list]->prev[list] = obj->prev[list];
    }
    obj->next[list] = 0;
    obj->prev[list] = 0;
}

static inline void
_aoi_list_insert_after(struct aoi *aoi, int list, struct aoi_object *obj,
                       struct aoi_object *p) {
    if (obj == p) {
        return;
    }
    obj->next[list] = p->next[list];
    obj->prev[list] = p;
    if (p->next[list]) {
        p->next[list]->prev[list] = obj;
    }
    p->next[list] = obj;
}

static inline void
_aoi_list_insert_before(struct aoi *aoi, int list, struct aoi_object *obj,
                        struct aoi_object *p) {
    if (obj == p) {
        return;
    }
    obj->next[list] = p;
    obj->prev[list] = p->prev[list];
    if (p->prev[list]) {
        p->prev[list]->next[list] = obj;
    } else {
        aoi->list[list] = obj;
    }
    p->prev[list] = obj;
}

static void
_aoi_update_list(struct aoi *aoi, struct aoi_object *obj, int d[2]) {
    int i;
    for (i = 0; i < 2; i++) {
        if (d[i] > 0) {
            struct aoi_object *p = obj;
            while (p) {
                if ((p->next[i] && (p->next[i]->p[i] > obj->p[i])) || !p->next[i]) {
                    if (p != obj) {
                        _aoi_list_erase(aoi, i, obj);
                        _aoi_list_insert_after(aoi, i, obj, p);
                    }
                    break;
                }
                p = p->next[i];
            }
        } else if (d[i] < 0) {
            struct aoi_object *p = obj;
            while (p) {
                if ((p->prev[i] && (p->prev[i]->p[i] < obj->p[i])) || !p->prev[i]) {
                    if (p != obj) {
                        _aoi_list_erase(aoi, i, obj);
                        _aoi_list_insert_before(aoi, i, obj, p);
                    }
                    break;
                }
                p = p->prev[i];
            }
        }
    }
}

AOI_API void
aoi_leave(struct aoi *aoi, int id) {
    struct aoi_object *obj;
    int i;

    obj = _aoi_object(aoi, id);
    if (!obj) {
        return;
    }

    /** remove object from x and y axis */
    for (i = 0; i < 2; i++) {
        _aoi_list_erase(aoi, i, obj);
    }
    free(obj->n_list);
    free(obj->o_list);
    memset(obj, 0, sizeof *obj);
    obj->type = AOI_OBJECT_INVALID;
}

AOI_API void *
aoi_ud(struct aoi *aoi, int id) {
    struct aoi_object *obj = _aoi_object(aoi, id);
    if (obj) {
        return obj->ud;
    }
    return 0;
}

AOI_API void
aoi_locate(struct aoi *aoi, int id, int x, int y) {
    struct aoi_object *obj;
    int d[2];

    obj = _aoi_object(aoi, id);
    if (!obj) {
        return;
    }
    d[0] = (x - obj->p[0]);
    d[1] = (y - obj->p[1]);
    obj->p[0] = x;
    obj->p[1] = y;
    /** update object position in x and y axis */
    _aoi_update_list(aoi, obj, d);
}

AOI_API void
aoi_move(struct aoi *aoi, int id, int x, int y) {
    struct aoi_object *obj;
    int i, d[2];
    float c;

    obj = _aoi_object(aoi, id);
    if (!obj) {
        return;
    }
    if (obj->speed <= 0 || (x == obj->p[0] && y == obj->p[1])) {
        return;
    }
    d[0] = x;
    d[1] = y;
    for (i = 0; i < 2; i++) {
        obj->sp[i] = obj->p[i];
        obj->dp[i] = d[i];
        d[i] -= obj->p[i];
    }
    c = sqrtf((float)(d[0] * d[0] + d[1] * d[1]));
    for (i = 0; i < 2; i++) {
        obj->d[i] = d[i] / c;
    }
    obj->e = (float)M_PI*obj->speed / c;
    obj->n_tick = (int)c / obj->speed;
    obj->p_tick = 0;
}

AOI_API void
aoi_speed(struct aoi *aoi, int id, int speed) {
    struct aoi_object *obj = _aoi_object(aoi, id);
    if (!obj) {
        return;
    }
    obj->speed = speed;
    if (obj->n_tick > 0) {
        /** object in moving, take effect change of speed */
        aoi_move(aoi, obj->id, obj->dp[0], obj->dp[1]);
    }
}

static void
_aoi_object_update(struct aoi *aoi, struct aoi_object *obj, int tick) {
    int i, ti;
    int d[2];

    ti = min(tick, obj->n_tick);
    obj->n_tick -= ti;
    obj->p_tick += ti;
    for (i = 0; i < 2; i++) {
        d[i] = ((obj->d[i] > 0) << 1) - 1;
    }
    if (obj->n_tick <= 0) {
        /** moving end, set cur position to destination */
        for (i = 0; i < 2; i++) {
            obj->p[i] = obj->dp[i];
        }
    } else {
        /** make moving step */
        float s = sinf(obj->e*obj->p_tick)*sinf(obj->e*obj->p_tick);
        for (i = 0; i < 2; i++) {
            obj->p[i] = (int)(obj->sp[i] + obj->d[i] * obj->speed*obj->p_tick
                              + ((i << 1) - 1) * obj->d[i] * s);
        }
    }
    _aoi_update_list(aoi, obj, d);
}

AOI_API void
aoi_update(struct aoi *aoi, int id, int tick) {
    struct aoi_object *obj = _aoi_object(aoi, id);
    if (!obj) {
        return;
    }
    if (obj->type == AOI_OBJECT_INVALID || obj->speed <= 0) {
        return;
    }
    if (obj->n_tick <= 0) {
        return;
    }
    _aoi_object_update(aoi, obj, tick);
}

AOI_API void
aoi_pos(struct aoi *aoi, int id, int *px, int *py) {
    struct aoi_object *obj = _aoi_object(aoi, id);
    if (!obj) {
        return;
    }
    *px = obj->p[0];
    *py = obj->p[1];
}

AOI_API int
aoi_moving(struct aoi *aoi, int id) {
    struct aoi_object *obj = _aoi_object(aoi, id);
    if (!obj) {
        return 0;
    }
    return obj->n_tick > 0;
}

static int *
_insert_list(int *list, int id) {
    int cur = list[0];
    if (cur >= list[1]) {
        list = (int *)realloc(list, (list[1] * 2 + 2) * sizeof(int));
        list[1] *= 2;
    }
    if (cur == 0 || id > list[cur + 1]) {
        list[cur + 2] = id;
        list[0]++;
    } else {
        int i = 2;
        while (i <= cur + 1) {
            if (id < list[i]) {
                int j;
                for (j = cur + 1; j >= i; j--) {
                    list[j + 1] = list[j];
                }
                list[i] = id;
                list[0]++;
                break;
            } else if (id == list[i]) {
                break;
            }
            i++;
        }
    }
    return list;
}

static int
_find_list(int *list, int id) {
    int cur = list[0];
    if (cur == 0 || id > list[cur + 1]) {
        return 0;
    } else {
        int i = 2;
        while (i <= cur + 1) {
            if (id == list[i]) {
                return 1;
            }
            i++;
        }
    }
    return 0;
}

AOI_API int
aoi_trigger(struct aoi *aoi, int id, int enter_r, int leave_r,
            struct aoi_event **list) {
    struct aoi_object *obj, *p;
    int *cur_list, i;
    int r = 0;

    obj = _aoi_object(aoi, id);
    if (!obj) {
        return r;
    }
    cur_list = obj->n_list;
    cur_list[0] = 0;
    /** only check x axis list is ok */
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            p = obj->prev[0];
        } else {
            p = obj->next[0];
        }
        /** get new version object list in x and y axis */
        while (p) {
            int dx = abs(obj->p[0] - p->p[0]);
            int dy = abs(obj->p[1] - p->p[1]);
            int d = dx * dx + dy * dy;
            if (dx > leave_r) {
                break;
            } else if (d <= enter_r * enter_r) {
                cur_list = _insert_list(cur_list, p->id);
            } else if (d <= leave_r * leave_r) {
                if (_find_list(obj->o_list, p->id)) {
                    cur_list = _insert_list(cur_list, p->id);
                }
            }
            if (i == 0) {
                p = p->prev[0];
            } else {
                p = p->next[0];
            }
        }
    }

    *list = aoi->elist;

    /** no object in old version list, all in array list is new enter */
    if (obj->o_list[0] == 0) {
        int i;
        for (i = 0; i < cur_list[0]; i++) {
            (*list)[r].id = cur_list[i + 2];
            (*list)[r].e = AOI_ENTER;
            r++;
        }
    } else {
        /** intersection and subtraction of list */
        int o_cnt = obj->o_list[0];
        int n_cnt = cur_list[0];
        int o_i = 2;
        int n_i = 2;
        for (;;) {
            int o, n;
            if (o_i > o_cnt + 1) {
                int i;
                for (i = n_i; i <= n_cnt + 1; i++) {
                    (*list)[r].id = cur_list[i];
                    (*list)[r].e = AOI_ENTER;
                    r++;
                }
                break;
            }
            if (n_i > n_cnt + 1) {
                int i;
                for (i = o_i; i <= o_cnt + 1; i++) {
                    (*list)[r].id = obj->o_list[i];
                    (*list)[r].e = AOI_LEAVE;
                    r++;
                }
                break;
            }
            o = obj->o_list[o_i];
            if (!_aoi_object(aoi, o)) {
                o_i++;
                continue;
            }
            n = cur_list[n_i];
            if (n < o) {
                (*list)[r].id = n;
                (*list)[r].e = AOI_ENTER;
                r++;
                n_i++;
            } else if (n == o) {
                o_i++;
                n_i++;
            } else {
                (*list)[r].id = o;
                (*list)[r].e = AOI_LEAVE;
                r++;
                o_i++;
            }
        }
    }

    /** change list version */
    obj->n_list = obj->o_list;
    obj->o_list = cur_list;
    return r;
}

AOI_API int
aoi_around(struct aoi *aoi, int id, int *list, int n) {
    struct aoi_object *obj;
    int i;

    obj = _aoi_object(aoi, id);
    if (!obj) {
        return 0;
    }
    n = n > obj->n_list[0] ? obj->n_list[0] : n;
    for (i = 0; i < n; i++) {
        list[i] = obj->n_list[i];
    }

    return n;
}

#endif // AOI_IMPLEMENTATION
