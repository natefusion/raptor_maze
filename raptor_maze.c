#define KERNAL_MODE
/* #define OLD_KERNAL */

#ifdef KERNAL_MODE

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/random.h>

#define reallocate(p, new_n, new_size, flags) krealloc((p), (new_n) * (new_size), (flags))
#define allocate_zero(new_n, new_size, flags) kzalloc((new_n) * (new_size), (flags))
#define print printk
#define free_mem kfree

#else

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#define reallocate(p, new_n, new_size, flags) realloc((p), (new_n)*(new_size))
#define allocate_zero(new_n, new_size, flags) calloc((new_n), (new_size))
#define print printf
#define KERN_INFO
#define free_mem free

#endif

#define Vec_define(T) typedef struct { T *data; int len; int capacity; } Vec_##T

#define Vec_append(vec, val)                                            \
    do {                                                                \
        if ((vec)->len >= (vec)->capacity)  {                           \
            if ((vec)->capacity == 0) (vec)->capacity = 2;              \
            else (vec)->capacity *= 2;                                  \
                                                                        \
            (vec)->data = reallocate((vec)->data,                       \
                                     (vec)->capacity,                   \
                                     sizeof(*(vec)->data),              \
                                     GFP_KERNEL);                       \
        }                                                               \
        (vec)->data[(vec)->len++] = (val);                              \
    } while(0)

#define Vec_union(set, x)                                       \
    do {                                                        \
        bool push = true;                                       \
        int i;                                                  \
        for (i = 0; i < (set)->len; ++i) {                      \
            if ((set)->data[i] == (x)) { push=false; break; }   \
        }                                                       \
        if (push) Vec_append((set), (x));                       \
    } while (0)

typedef struct {
    int index;
    int weight;
} AdjVertex;

typedef struct {
    int u;
    int v;
    int weight;
} Edge;

Vec_define(Edge);
Vec_define(int);
Vec_define(AdjVertex);
Vec_define(Vec_AdjVertex);
Vec_define(char);

typedef Vec_char String;
typedef Vec_Vec_AdjVertex Graph;

void make_grid(Vec_Edge *, int width, int height);
void randomly_sort(Vec_Edge *e);
bool is_safe_to_insert(Vec_int set, Edge e);
void Graph_insert(Graph *g, Edge e);
int Graph_get_weight(Vec_AdjVertex av, int index);
void kruskal_maze(Vec_Edge *e, Graph *mst);
void make_maze(String *s, Graph *mst, int width, int height);
int rand(void);

#ifdef KERNAL_MODE
ssize_t proc_read(struct file *file, char *buf, size_t count, loff_t *pos);
int init(void);
void deinit(void);
ssize_t do_maze(struct file *file, char __user *usr_buf, size_t count, loff_t *pos);

static struct file_operations proc_ops = {
    .owner = THIS_MODULE,
    .read = do_maze,
};
#endif

int rand(void) {
    int i;
    get_random_bytes(&i, sizeof(i));
    i = (i/2) + INT_MAX/2; // range of [0, INT_MAX] probably
    return i;
}

void make_grid(Vec_Edge *e, int width, int height) {
    int row = 0;
    int col = 0;
    int i;
    for (i = 0; i < width*height; ++i) {
        row = i / width;
        col = i % width;
        
        if (col < width-1) {
            Edge x; x.u=i; x.v=i+1; x.weight=rand() % 2;
            Vec_append(e, x);
        }
        if (row < height-1) {
            Edge x; x.u=i; x.v=i+width; x.weight=rand() % 2;
            Vec_append(e, x);
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

bool is_safe_to_insert(Vec_int set, Edge e) {
    int strike = 0;
    int i;
    for (i = 0; i < set.len; ++i) {
        int vertex = set.data[i];
        if (vertex == e.u) ++strike;
        if (vertex == e.v) ++strike;
        if (strike >= 2) return false;
     }

    return true;
}

void Graph_insert(Graph *g, Edge e) {
    bool push_u = true;
    bool push_v = true;
    Vec_AdjVertex eu = g->data[e.u];
    Vec_AdjVertex ev = g->data[e.v];

    int i;
    for (i = 0; i < eu.len; ++i) if (eu.data[i].index == e.v) push_u = false;
    for (i = 0; i < ev.len; ++i) if (ev.data[i].index == e.u) push_v = false;

    if (push_u) { AdjVertex av; av.index=e.v; av.weight=e.weight; Vec_append(&eu, av); }
    if (push_v) { AdjVertex av; av.index=e.u; av.weight=e.weight; Vec_append(&ev, av); }
    
    g->data[e.u] = eu;
    g->data[e.v] = ev;
}

int Graph_get_weight(Vec_AdjVertex av, int index) {
    int i;
    for (i = 0; i < av.len; ++i) {
        if (av.data[i].index == index) return av.data[i].weight;
    }
    
    return 0;
}

void kruskal_maze(Vec_Edge *e, Graph *mst) {
    int *data;
    Vec_int vertex_set;
    int i;
    
    randomly_sort(e);

    data = allocate_zero(e->len, sizeof(int), GFP_KERNEL);
    
    vertex_set.data = data;
    vertex_set.len = 0;
    vertex_set.capacity=e->len;

    for (i = 0; i < e->len; ++i) {
        if (is_safe_to_insert(vertex_set, e->data[i])) {
            Graph_insert(mst, e->data[i]);

            Vec_union(&vertex_set, e->data[i].u);
            Vec_union(&vertex_set, e->data[i].v);
        }
    }

    free_mem(data);
}

void make_maze(String *s, Graph *mst, int width, int height) {
    int k = 'A';
    int i;
    for (i = 0; i < height; ++i) {
        int j;
        for (j = 0; j < width; ++j) {
            int index = i*width+j;
            bool last_column = (j+1) % width == 0;
            int eastern_weight = Graph_get_weight(mst->data[index], index+1);
            
            /* Vec_append(s, k); */
            Vec_append(s, ' ');
            if (!last_column && eastern_weight != 0) Vec_append(s, ' ');
            else Vec_append(s, '#');
            k++;
        }

        Vec_append(s, '\n');
        if (i < height - 1) {
            int j;
            for (j = 0; j < width; ++j) {
                int index = i*width+j;
                int southern_weight = Graph_get_weight(mst->data[index], index+width);
                if (southern_weight != 0) Vec_append(s, ' ');
                else Vec_append(s, '#');
                Vec_append(s, '#');
            }
        }
        Vec_append(s, '\n');
    }
    Vec_append(s, '\0');
}

#ifdef KERNAL_MODE
ssize_t do_maze(struct file *file, char __user *usr_buf, size_t count, loff_t *pos)
#else
    int main(void)
#endif
{
    int width = 80;
    int height = 28;
    Vec_Edge grid = {0};
    Graph minimum_spanning_tree;
    String buffer = {0};
    #ifdef KERNAL_MODE
    unsigned long bytes_not_copied;
    #endif

    minimum_spanning_tree.data = allocate_zero(width*height, sizeof(Vec_AdjVertex), GFP_KERNEL);
    minimum_spanning_tree.len = width*height;
    minimum_spanning_tree.capacity=width*height;

    #ifndef KERNAL_MODE
    srand(time(NULL));
    #endif
    
    make_grid(&grid, width, height);
    kruskal_maze(&grid, &minimum_spanning_tree);
    make_maze(&buffer, &minimum_spanning_tree, width, height);

    #ifdef KERNAL_MODE
    bytes_not_copied = copy_to_user(usr_buf, buffer.data, buffer.len);
    if (bytes_not_copied > 0) {
        print(KERN_INFO "proc/raptor_maze: UH OH\n");
    }
    #else
    printf("%s", buffer.data);
    #endif    
    
    free_mem(buffer.data);
    free_mem(minimum_spanning_tree.data);
    free_mem(grid.data);

    #ifdef KERNAL_MODE
    return buffer.len;
    #else
    return 0;
    #endif
}

#ifdef KERNAL_MODE
int init(void) {
    proc_create("raptor_maze", 0, NULL,
                #ifndef OLD_KERNAL
                (struct proc_ops*)
                #endif
                &proc_ops
        );
    print(KERN_INFO "/proc/raptor_maze created\n");
    return 0;
}

void deinit(void) {
    remove_proc_entry("raptor_maze", NULL);
    print(KERN_INFO "proc/raptor_maze removed\n");
}


module_init(init);
module_exit(deinit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Maze, but kernal");
MODULE_AUTHOR("Nathan Piel");

#endif
