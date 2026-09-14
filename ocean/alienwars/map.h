#ifndef ALIENWARS_MAP_H
#define ALIENWARS_MAP_H

/* Shared, renderer-independent terrain and navigation. Integer-only generation
 * keeps seeds identical on native CPUs and WebAssembly. No global RNG state. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define AW_SIZE 48
#define AW_VERT (AW_SIZE + 1)
#define AW_CELLS (AW_SIZE * AW_SIZE)
#define AW_TILES 31
#define AW_VERSION 1
#define AW_MAX_RETRIES 8

typedef struct {
    uint8_t corner[4]; /* NW, NE, SE, SW; 0 water, 1 lowland, 2 plateau */
    uint8_t weight;
} AwTile;

typedef struct {
    uint32_t seed, rng, wave[AW_CELLS], compatible[AW_TILES][4];
    AwTile tiles[AW_TILES];
    uint8_t allowed[AW_VERT * AW_VERT], preferred[AW_VERT * AW_VERT];
    int8_t tile[AW_CELLS], ramp[AW_CELLS];
    uint8_t walkable[AW_CELLS], reachable[AW_CELLS];
    int order[AW_CELLS], order_count, decisions, reductions, attempts;
    int path[AW_CELLS], path_length, walk_count, reached_count;
    int resources[4], spawns[2], center, valid;
    uint32_t hash;
} AwMap;

static uint32_t aw_hash(uint32_t x) {
    x ^= x >> 16;
    x *= UINT32_C(0x7feb352d);
    x ^= x >> 15;
    x *= UINT32_C(0x846ca68b);
    return x ^ (x >> 16);
}

static uint32_t aw_random(AwMap *m) {
    m->rng += UINT32_C(0x9e3779b9);
    return aw_hash(m->rng);
}

static int aw_noise(uint32_t seed, int x, int z, int scale) {
    int gx = x / scale, gz = z / scale;
    int fx = x % scale, fz = z % scale;
    int v[4];
    for (int i = 0; i < 4; i++) {
        uint32_t h = seed ^ (uint32_t)(gx + (i & 1)) * 73856093u;
        h ^= (uint32_t)(gz + (i >> 1)) * 19349663u;
        v[i] = (int)(aw_hash(h) % 201) - 100;
    }
    int a = v[0] * (scale - fx) + v[1] * fx;
    int b = v[2] * (scale - fx) + v[3] * fx;
    return (a * (scale - fz) + b * fz) / (scale * scale);
}

static int aw_count(uint32_t bits) {
    return __builtin_popcount(bits);
}

static int aw_only(uint32_t bits) {
    return bits ? __builtin_ctz(bits) : -1;
}

static int aw_neighbor(int cell, int d) {
    int x = cell % AW_SIZE, z = cell / AW_SIZE;
    if (d == 0) return z ? cell - AW_SIZE : -1;
    if (d == 1) return x + 1 < AW_SIZE ? cell + 1 : -1;
    if (d == 2) return z + 1 < AW_SIZE ? cell + AW_SIZE : -1;
    return x ? cell - 1 : -1;
}

static void aw_catalog(AwMap *m) {
    int n = 0;
    for (int base = 0; base < 2; base++) {
        for (int pattern = 0; pattern < 16; pattern++) {
            if (base == 1 && pattern == 0) continue; /* duplicate lowland */
            AwTile *t = &m->tiles[n++];
            for (int c = 0; c < 4; c++) t->corner[c] = base + ((pattern >> c) & 1);
            t->weight = (pattern == 0 || pattern == 15) ? 7 : 2;
            if (pattern == 5 || pattern == 10) t->weight = 1;
        }
    }
    const int edge[4][2] = {{0,1}, {1,2}, {3,2}, {0,3}};
    for (int a = 0; a < AW_TILES; a++) {
        for (int d = 0; d < 4; d++) {
            m->compatible[a][d] = 0;
            for (int b = 0; b < AW_TILES; b++) {
                int opposite = (d + 2) % 4;
                if (m->tiles[a].corner[edge[d][0]] == m->tiles[b].corner[edge[opposite][0]] &&
                    m->tiles[a].corner[edge[d][1]] == m->tiles[b].corner[edge[opposite][1]]) {
                    m->compatible[a][d] |= UINT32_C(1) << b;
                }
            }
        }
    }
}

static void aw_fix(AwMap *m, int x, int z, int height) {
    if (x < 1 || z < 1 || x >= AW_SIZE || z >= AW_SIZE) return;
    m->allowed[z * AW_VERT + x] = 1 << height;
    m->preferred[z * AW_VERT + x] = height;
}

static void aw_disk(AwMap *m, int cx, int cz, int radius, int height) {
    for (int z = cz - radius; z <= cz + radius; z++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if ((x-cx)*(x-cx) + (z-cz)*(z-cz) <= radius*radius) aw_fix(m, x, z, height);
        }
    }
}

static void aw_route(AwMap *m, int x0, int z0, int x1, int z1, int radius) {
    int dx = abs(x1-x0), dz = abs(z1-z0), steps = dx > dz ? dx : dz;
    if (steps == 0) steps = 1;
    for (int i = 0; i <= steps; i++) {
        aw_disk(m, x0 + (x1-x0)*i/steps, z0 + (z1-z0)*i/steps, radius, 1);
    }
}

static void aw_layout(AwMap *m) {
    /* A macro graph anchors playable space; uncertainty remains on coastlines
     * and plateau boundaries. WFC chooses the compatible boundary geometry. */
    const int islands[7][3] = {{10,12,10},{38,36,10},{24,24,12},
        {13,34,8},{35,14,8},{21,12,7},{27,36,7}};
    for (int z = 0; z < AW_VERT; z++) {
        for (int x = 0; x < AW_VERT; x++) {
            int land = -10000;
            for (int i = 0; i < 7; i++) {
                int radius = islands[i][2] + (int)(aw_hash(m->seed + i * 711u) % 3) - 1;
                int dx = x - islands[i][0], dz = z - islands[i][1];
                int field = radius*radius - dx*dx - dz*dz;
                if (field > land) land = field;
            }
            land += aw_noise(m->seed, x, z, 5) / 4 + aw_noise(m->seed ^ 913u, x, z, 2) / 8;
            int high = -10000;
            for (int i = 0; i < 2; i++) {
                int cx = i ? 38 : 10, cz = i ? 36 : 12;
                int dx = x-cx, dz = z-cz;
                int f = 38 - dx*dx - dz*dz;
                if (f > high) high = f;
            }
            high += aw_noise(m->seed ^ 471u, x, z, 4) / 9;
            int v = z * AW_VERT + x;
            m->allowed[v] = land < -12 ? 1 : land < 12 ? 3 : 2;
            m->preferred[v] = land < 0 ? 0 : 1;
            if (land > 24 && high > -8) {
                m->allowed[v] = high > 8 ? 4 : 6;
                m->preferred[v] = high > 0 ? 2 : 1;
            }
            if (x < 2 || z < 2 || x > 46 || z > 46) {
                m->allowed[v] = 1;
                m->preferred[v] = 0;
            }
        }
    }
    aw_route(m, 18, 12, 24, 24, 3);
    aw_route(m, 24, 24, 30, 36, 3);
    aw_route(m, 24, 24, 13, 34, 3);
    aw_route(m, 24, 24, 35, 14, 3);
    aw_disk(m, 24, 24, 6, 1);
    aw_disk(m, 13, 34, 4, 1);
    aw_disk(m, 35, 14, 4, 1);
    aw_disk(m, 10, 12, 4, 2);
    aw_disk(m, 38, 36, 4, 2);
    memset(m->ramp, -1, sizeof(m->ramp));
    for (int r = 0; r < 2; r++) {
        int startx = r ? 30 : 14, startz = r ? 35 : 11;
        /* Four-cell run, three-cell width. Substrate heights remain WFC tiles;
         * the semantic ramp adds quarter-level surfaces for render and nav. */
        for (int z = startz; z <= startz + 3; z++) {
            for (int x = startx; x <= startx + 4; x++) {
                int high = r ? (x >= startx + 2) : (x <= startx + 2);
                aw_fix(m, x, z, high ? 2 : 1);
            }
        }
        for (int z = startz; z < startz + 3; z++) {
            for (int x = startx; x < startx + 4; x++) m->ramp[z*AW_SIZE+x] = r;
        }
        aw_route(m, r ? 28 : 19, startz+1, r ? 30 : 20, startz+1, 2);
        /* Explicit flat landings, kept outside the sloping strip. */
        for (int z = startz; z <= startz+3; z++) {
            aw_fix(m, r ? 30 : 18, z, 1);
            aw_fix(m, r ? 29 : 19, z, 1);
            aw_fix(m, r ? 34 : 14, z, 2);
            aw_fix(m, r ? 35 : 13, z, 2);
        }
    }
    m->spawns[0] = 12*AW_SIZE+10;
    m->spawns[1] = 36*AW_SIZE+38;
    m->resources[0] = 11*AW_SIZE+8;
    m->resources[1] = 37*AW_SIZE+40;
    m->resources[2] = 34*AW_SIZE+13;
    m->resources[3] = 14*AW_SIZE+35;
    m->center = 24*AW_SIZE+24;
}

static void aw_record(AwMap *m, int cell) {
    if (m->tile[cell] < 0 && aw_count(m->wave[cell]) == 1) {
        m->tile[cell] = aw_only(m->wave[cell]);
        m->order[m->order_count++] = cell;
    }
}

static int aw_propagate(AwMap *m, int initial) {
    int queue[AW_CELLS], pending[AW_CELLS] = {0};
    int head = 0, tail = 0, count = 0;
    for (int c = 0; c < AW_CELLS; c++) {
        if (initial >= 0 && c != initial) continue;
        queue[tail] = c;
        tail = (tail+1) % AW_CELLS;
        pending[c] = 1;
        count++;
    }
    while (count) {
        int cell = queue[head];
        head = (head+1) % AW_CELLS;
        count--;
        pending[cell] = 0;
        for (int d = 0; d < 4; d++) {
            int nb = aw_neighbor(cell, d);
            if (nb < 0) continue;
            uint32_t supports = 0, options = m->wave[cell];
            while (options) {
                int t = aw_only(options);
                supports |= m->compatible[t][d];
                options &= options-1;
            }
            uint32_t next = m->wave[nb] & supports;
            if (next == m->wave[nb]) continue;
            if (!next) return 0;
            m->reductions += aw_count(m->wave[nb]) - aw_count(next);
            m->wave[nb] = next;
            aw_record(m, nb);
            if (!pending[nb]) {
                queue[tail] = nb;
                tail = (tail+1) % AW_CELLS;
                count++;
                pending[nb] = 1;
            }
        }
    }
    return 1;
}

static int aw_solve(AwMap *m) {
    memset(m->tile, -1, sizeof(m->tile));
    m->order_count = m->decisions = m->reductions = 0;
    const int offsets[4] = {0,1,AW_VERT+1,AW_VERT};
    for (int c = 0; c < AW_CELLS; c++) {
        int v = c/AW_SIZE * AW_VERT + c%AW_SIZE;
        m->wave[c] = 0;
        for (int t = 0; t < AW_TILES; t++) {
            int allowed = 1;
            for (int k = 0; k < 4; k++) {
                if (!(m->allowed[v+offsets[k]] & (1 << m->tiles[t].corner[k]))) allowed = 0;
            }
            if (allowed) m->wave[c] |= UINT32_C(1) << t;
        }
        if (!m->wave[c]) return 0;
        aw_record(m, c);
    }
    if (!aw_propagate(m, -1)) return 0;
    while (1) {
        /* Minimum remaining values: an integer entropy heuristic. Seeded
         * reservoir sampling breaks ties without platform-dependent log(). */
        int selected = -1, minimum = 32, ties = 0;
        for (int c = 0; c < AW_CELLS; c++) {
            int n = aw_count(m->wave[c]);
            if (n <= 1 || n > minimum) continue;
            if (n < minimum) { minimum = n; ties = 0; }
            ties++;
            if (aw_random(m) % (uint32_t)ties == 0) selected = c;
        }
        if (selected < 0) return 1;
        int weights[AW_TILES] = {0}, total = 0;
        int v = selected/AW_SIZE * AW_VERT + selected%AW_SIZE;
        for (int t = 0; t < AW_TILES; t++) {
            if (!(m->wave[selected] & (UINT32_C(1) << t))) continue;
            int weight = m->tiles[t].weight;
            for (int k = 0; k < 4; k++) {
                if (m->tiles[t].corner[k] == m->preferred[v+offsets[k]]) weight += 4;
            }
            weights[t] = weight;
            total += weight;
        }
        int choice = (int)(aw_random(m) % (uint32_t)total), t = 0;
        while (choice >= weights[t]) choice -= weights[t++];
        m->wave[selected] = UINT32_C(1) << t;
        m->decisions++;
        aw_record(m, selected);
        if (!aw_propagate(m, selected)) return 0;
    }
}

static int aw_corner_q(const AwMap *m, int cell, int corner) {
    int r = m->ramp[cell];
    if (r >= 0) {
        int x = cell % AW_SIZE + (corner == 1 || corner == 2);
        return r ? 4 + x - 30 : 8 - (x - 14);
    }
    return 4 * m->tiles[(int)m->tile[cell]].corner[corner];
}

static int aw_open(const AwMap *m, int a, int b) {
    if (a < 0 || b < 0 || a >= AW_CELLS || b >= AW_CELLS) return 0;
    if (!m->walkable[a] || !m->walkable[b]) return 0;
    const int edge[4][2] = {{0,1},{1,2},{3,2},{0,3}};
    for (int d = 0; d < 4; d++) {
        if (aw_neighbor(a,d) != b) continue;
        int op = (d+2)%4;
        return aw_corner_q(m,a,edge[d][0]) == aw_corner_q(m,b,edge[op][0]) &&
               aw_corner_q(m,a,edge[d][1]) == aw_corner_q(m,b,edge[op][1]);
    }
    return 0;
}

static int aw_find_path(const AwMap *m, int start, int goal, int *path) {
    if (start < 0 || goal < 0 || start >= AW_CELLS || goal >= AW_CELLS ||
        !m->walkable[start] || !m->walkable[goal]) return 0;
    int parent[AW_CELLS], queue[AW_CELLS], head = 0, tail = 0;
    for (int c = 0; c < AW_CELLS; c++) parent[c] = -1;
    queue[tail++] = start;
    parent[start] = start;
    while (head < tail && parent[goal] < 0) {
        int c = queue[head++];
        for (int d = 0; d < 4; d++) {
            int nb = aw_neighbor(c,d);
            if (nb < 0 || parent[nb] >= 0 || !aw_open(m,c,nb)) continue;
            parent[nb] = c;
            queue[tail++] = nb;
        }
    }
    if (parent[goal] < 0) return 0;
    int n = 0, c = goal;
    while (c != start) { path[n++] = c; c = parent[c]; }
    path[n++] = start;
    for (int i = 0; i < n/2; i++) {
        int swap = path[i]; path[i] = path[n-1-i]; path[n-1-i] = swap;
    }
    return n;
}

static int aw_validate(AwMap *m) {
    m->walk_count = m->reached_count = 0;
    memset(m->reachable, 0, sizeof(m->reachable));
    for (int c = 0; c < AW_CELLS; c++) {
        if (m->tile[c] < 0 || m->tile[c] >= AW_TILES) return 0;
        AwTile *t = &m->tiles[(int)m->tile[c]];
        m->walkable[c] = m->ramp[c] >= 0 || (t->corner[0] > 0 &&
            t->corner[0] == t->corner[1] && t->corner[0] == t->corner[2] && t->corner[0] == t->corner[3]);
        m->walk_count += m->walkable[c];
        for (int d = 0; d < 4; d++) {
            int nb = aw_neighbor(c,d);
            if (nb >= 0 && !(m->compatible[(int)m->tile[c]][d] & (1u << m->tile[nb]))) return 0;
        }
    }
    int queue[AW_CELLS], head = 0, tail = 0;
    if (!m->walkable[m->spawns[0]]) return 0;
    queue[tail++] = m->spawns[0];
    m->reachable[m->spawns[0]] = 1;
    while (head < tail) {
        int c = queue[head++];
        for (int d = 0; d < 4; d++) {
            int nb = aw_neighbor(c,d);
            if (nb < 0 || m->reachable[nb] || !aw_open(m,c,nb)) continue;
            m->reachable[nb] = 1;
            queue[tail++] = nb;
        }
    }
    m->reached_count = tail;
    if (!m->reachable[m->spawns[1]] || !m->reachable[m->center]) return 0;
    for (int i = 0; i < 4; i++) if (!m->reachable[m->resources[i]]) return 0;
    /* All three lanes of each planned ramp must connect end-to-end. */
    for (int r = 0; r < 2; r++) {
        int x = r ? 29 : 13, z = r ? 35 : 11;
        for (int lane = 0; lane < 3; lane++) {
            for (int i = 0; i < 5; i++) {
                int a = (z+lane)*AW_SIZE + x+i;
                if (!aw_open(m,a,a+1)) return 0;
            }
        }
    }
    m->path_length = aw_find_path(m,m->spawns[0],m->spawns[1],m->path);
    return m->path_length > 0;
}

static uint32_t aw_fingerprint(const AwMap *m) {
    uint32_t hash = 2166136261u;
    for (int c = 0; c < AW_CELLS; c++) {
        hash = (hash ^ (uint8_t)m->tile[c]) * 16777619u;
        hash = (hash ^ (uint8_t)m->ramp[c]) * 16777619u;
    }
    return hash;
}

static int aw_generate(AwMap *m, uint32_t seed) {
    memset(m, 0, sizeof(*m));
    m->seed = seed;
    aw_catalog(m);
    for (int attempt = 0; attempt < AW_MAX_RETRIES; attempt++) {
        m->attempts = attempt+1;
        m->rng = aw_hash(seed ^ (uint32_t)attempt * 9176u);
        aw_layout(m);
        if (!aw_solve(m) || !aw_validate(m)) continue;
        m->hash = aw_fingerprint(m);
        m->valid = 1;
        return 1;
    }
    m->valid = 0;
    return 0;
}

#endif
