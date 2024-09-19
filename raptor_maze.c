/* #define KERNAL_MODE */

#ifdef KERNAL_MODE

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/sprintf.h>

#define reallocate(p, new_n, new_size, flags) krealloc((p), (new_n) * (new_size), (flags))
#define allocate_zero(new_n, new_size, flags) kzalloc((new_n) * (new_size), (flags))
#define print printk
#define free_mem kfree

#else

#define GFP_KERNEL (void)0

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>

#define reallocate(p, new_n, new_size, flags) realloc((p), (new_n)*(new_size))
#define allocate_zero(new_n, new_size, flags) calloc((new_n), (new_size))
#define print printf
#define KERN_INFO
#define free_mem free
#endif

#define Vec_define(T) typedef struct { T *data; int len; int capacity; } Vec_##T

#define Vec_alloc_full(vec, n)                                      \
    do {                                                            \
        (vec)->data = Arena_alloc(&arena, n, sizeof(*(vec)->data)); \
        (vec)->len = n;                                             \
        (vec)->capacity = n;                                        \
    } while (0)

#define Vec_alloc_empty(vec, n)                                     \
    do {                                                            \
        (vec)->data = Arena_alloc(&arena, n, sizeof(*(vec)->data)); \
        (vec)->len = 0;                                             \
        (vec)->capacity = n;                                        \
    } while (0)

#define Vec_push(vec, val) (vec)->data[(vec)->len++] = (val)
    
#define Vec_push_extend(vec, val)                                       \
    do {                                                                \
        if ((vec)->len >= (vec)->capacity)  {                           \
            if ((vec)->capacity == 0) (vec)->capacity = 2;              \
            else (vec)->capacity *= 2;                                  \
            (vec)->data = Arena_realloc(&arena,                         \
                                        (vec)->data,                    \
                                        (vec)->capacity,                \
                                        sizeof(*(vec)->data));          \
        }                                                               \
        Vec_push(vec, val);                                             \
    } while(0)

typedef struct {
    int u;
    int v;
} Edge;

typedef struct {
    int data[4];
    int len;
} Vec4_int;

typedef void* voidptr;

Vec_define(voidptr);
Vec_define(Edge);
Vec_define(int);
Vec_define(Vec_int);
Vec_define(Vec4_int);
Vec_define(char);

typedef Vec_char String;
typedef Vec_Vec4_int Graph;
typedef Vec_voidptr Arena;

static Arena arena;
static int maze_width = 10;
static int maze_height = 10;

#ifdef KERNAL_MODE
static struct file_operations proc_ops = {
    .owner = THIS_MODULE,
    .read = do_maze,
    .write = get_input,
};
#endif

void *Arena_alloc(Arena *arena, int n, int size);
void *Arena_realloc(Arena *arena, void *p, int n, int size);
void Arena_init(Arena *arena);
void Arena_deinit(Arena *arena);

void make_grid(Vec_Edge *, int width, int height);
void randomly_sort(Vec_Edge *e);
int find_set(Vec_Vec_int set, int uv);
void Graph_insert(Graph *g, Edge e);
bool Graph_get_adj(Vec4_int av, int index);
void kruskal_maze(Vec_Edge *e, Graph *mst);
void make_maze_line(String *s, Graph *mst, int width, int height);
void make_maze_hash(String *s, Graph *mst, int width, int height);
void print_graph(String *s, Graph g);
void print_edges(String *s, Vec_Edge e);

#ifdef KERNAL_MODE
int rand(void);
int init(void);
void deinit(void);
ssize_t do_maze(struct file *file, char __user *usr_buf, size_t count, loff_t *pos);
ssize_t get_input(struct file *file, const char __user *usr_buf, size_t count, loff_t *pos);
#endif

void *Arena_alloc(Arena *arena, int n, int size) {
    void *p;
    if (arena->len >= arena->capacity) {
        if (arena->capacity == 0) arena->capacity = 2;
        else arena->capacity *= 2;
        arena->data = reallocate(arena->data, arena->capacity, sizeof(void*), GFP_KERNEL);
    }
    p = allocate_zero(n, size, GFP_KERNEL);
    arena->data[arena->len++] = p;
    
    return p;
}

void *Arena_realloc(Arena *arena, void *p, int n, int size) {
    if (p) {
        int i;
        for (i = 0; i < arena->len; ++i) {
            if (arena->data[i] == p) {
                void *new_p = reallocate(p, n, size, GFP_KERNEL);
                arena->data[i] = new_p;
                return new_p;
            }
        }
    }

    return Arena_alloc(arena, n, size);
}

void Arena_init(Arena *arena) {
    arena->data = 0;
    arena->len = 0;
    arena->capacity = 0;
}

void Arena_deinit(Arena *arena) {
    int i;
    for (i = 0; i < arena->len; ++i) {
        free_mem(arena->data[i]);
    }
    
    free_mem(arena->data);
}

#ifdef KERNAL_MODE
int rand(void) {
    int i;
    get_random_bytes(&i, sizeof(i));
    i = (i/2) + INT_MAX/2; /* range of [0, INT_MAX] probably */
    return i;
}
#endif

void make_grid(Vec_Edge *e, int width, int height) {
    int row = 0;
    int col = 0;
    int i;

    Vec_alloc_empty(e, width*(height-1) + (width-1)*height);
    for (i = 0; i < width*height; ++i) {
        row = i / width;
        col = i % width;
        
        if (col < width-1) {
            Edge x; x.u=i; x.v=i+1;
            Vec_push(e, x);
        }
        if (row < height-1) {
            Edge x; x.u=i; x.v=i+width;
            Vec_push(e, x);
        }
    }
}

void randomly_sort(Vec_Edge *e) {
    int i;
    for (i = 0; i <= e->len-2; ++i) {
        int j = rand() % (e->len - i) + i;
        Edge tmp = e->data[i];
        e->data[i] = e->data[j];
        e->data[j] = tmp;
    }
}

int find_set(Vec_Vec_int set, int uv) {
    int i;
    for (i = 0; i < set.len; ++i) {
        Vec_int *inner = &set.data[i];
        int j;
        for (j = 0; j < inner->len; ++j) {
            if (inner->data[j] == uv) return i;                
        }
    }

    return 0; 
}

void Graph_insert(Graph *g, Edge e) {
    Vec_push(&g->data[e.u], e.v);
    Vec_push(&g->data[e.v], e.u);
}

bool Graph_get_adj(Vec4_int av, int index) {
    int i;
    for (i = 0; i < 4; ++i) {
        if (av.data[i] == index) return true;
    }
    
    return false;
}

void kruskal_maze(Vec_Edge *e, Graph *mst) {
    Vec_Vec_int vertex_sets = {0};
    int i;

    randomly_sort(e);

    Vec_alloc_full(mst, maze_width*maze_height);
    Vec_alloc_full(&vertex_sets, mst->len);

    for (i = 0; i < vertex_sets.len; ++i) {
        Vec_int *set = &vertex_sets.data[i];
        Vec_alloc_empty(set, mst->len);        
        Vec_push(set, i);
    }

    for (i = 0; i < e->len; ++i) {
        int u_set_idx = find_set(vertex_sets, e->data[i].u);
        int v_set_idx = find_set(vertex_sets, e->data[i].v);
        if (u_set_idx != v_set_idx) {
            int j;
            Vec_int *v_set = &vertex_sets.data[v_set_idx];
            Vec_int *u_set = &vertex_sets.data[u_set_idx];
            
            Graph_insert(mst, e->data[i]);
            
            for (j = 0; j < v_set->len; ++j) {
                Vec_push(u_set, v_set->data[j]);
            }
            v_set->len = 0;
        }
    }
}

void make_maze_line(String *s, Graph *mst, int width, int height) {
    int i;

    Vec_alloc_empty(s, maze_width*2*maze_height);
    
    Vec_push(s, ' ');
    for (i = 0; i < width*2-1; ++i) Vec_push(s, '_');
    Vec_push(s, '\n');
    
    for (i = 0; i < height; ++i) {
        int j;
        
        Vec_push(s, '|');
        for (j = 0; j < width; ++j) {
            int index = i*width+j;
            
            bool eastern_edge = Graph_get_adj(mst->data[index], index+1);
            bool southern_edge = Graph_get_adj(mst->data[index], index+width);
            
            if (southern_edge) Vec_push(s, ' ');
            else Vec_push(s, '_');
            
            if (eastern_edge) {
                bool next_southern = Graph_get_adj(mst->data[index+1], index+1+width);
                if (next_southern) Vec_push(s, ' ');
                else Vec_push(s, '_');
            } else {
                Vec_push(s, '|');
            }
        }
        
        Vec_push(s, '\n');
    }

    Vec_push(s, '\0');
}

#ifdef KERNAL_MODE
ssize_t do_maze(struct file *file, char __user *usr_buf, size_t count, loff_t *pos)
#else
    int main(void)
#endif
    {
        Vec_Edge grid = {0};
        Graph spanning_tree = {0};
        String buffer = {0};
#ifdef KERNAL_MODE
        unsigned long bytes_not_copied;
#endif
        Arena_init(&arena);
/* #ifndef KERNAL_MODE */
/*         scanf("%d%d", &maze_width, &maze_height); */
/* #endif */
        
#ifndef KERNAL_MODE
        srand(time(NULL));
#endif
        make_grid(&grid, maze_width, maze_height);
        kruskal_maze(&grid, &spanning_tree);
        make_maze_line(&buffer, &spanning_tree, maze_width, maze_height);

#ifdef KERNAL_MODE
        bytes_not_copied = copy_to_user(usr_buf, buffer.data, buffer.len);
        if (bytes_not_copied > 0) {
            print(KERN_INFO "proc/raptor_maze: UH OH\n");
        }
#else
        printf("%s", buffer.data);
#endif    
    
        Arena_deinit(&arena);

#ifdef KERNAL_MODE
        return buffer.len;
#else
        return 0;
#endif
}

#ifdef KERNAL_MODE
int init(void) {
    proc_create("raptor_maze", 0, NULL, &proc_ops);
    print(KERN_INFO "/proc/raptor_maze created\n");
    return 0;
}

void deinit(void) {
    remove_proc_entry("raptor_maze", NULL);
    print(KERN_INFO "proc/raptor_maze removed\n");
}

ssize_t get_input(struct file *file, const char __user *usr_buf, size_t count, loff_t *pos) {
    sscanf(usr_buf, "%d%d", &maze_width, &maze_height);
    return count; /* idk what it wants me to return */
}

module_init(init);
module_exit(deinit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Maze, but kernal");
MODULE_AUTHOR("Nathan Piel");

#endif
