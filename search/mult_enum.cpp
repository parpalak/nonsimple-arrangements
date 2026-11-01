// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

// Search over pseudoline arrangements with vertices of any multiplicity, n <= 14,
// for Section 3 of R. Parpalak, D. Utkin, "Non-simple Arrangements of Lines and
// Pseudolines with Many Triangles". Lemma numbers in the comments refer to this
// preprint. Options and output format: README.md.
//
// Without options: the algorithm of Section 3. --pruning adds the prunings of
// Appendix C and does not change the words (Lemma C.12). --first-fixed (Lemma 3.2)
// and --reduced (Corollary 3.10, Lemma C.14) keep the existence verdict, not the words.
//
// Exit codes: 0 normal, 1 bad arguments, 2 INCOMPLETE (limit or signal, no
// verdict), 255 a segment-count lemma is violated or (with --stop-on-word) a word
// is found; on 255 xargs -P stops dispatching.
//
// Build:  g++ -O3 -march=native -std=c++17 -o multenum mult_enum.cpp
// After changing the table, check that an entry is read and written by one
// 16-byte operation (see Entry):
//   objdump -d multenum | grep vmovdqa     # entry load and store
//   nm -u multenum | grep __atomic         # empty: no libatomic
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <climits>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <emmintrin.h>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

// ------------------------------------------------------------ parameters ---
static int N; // n, the number of wires
static int TARGET_FACES; // target K: a3 >= K
static int BUDGET_FACES; // budget B = C(n-1, 2) - K
static int TOTAL_PAIRS; // C(n, 2), the length of a full word in pairs
static int MAX_MULT;
static int REQUIRE_MULT = 0;
static int PREFIX_DEPTH = -1;
// Word grammar (Section 3).
static bool FIRST_FIXED = false;
static bool FORCED_DESCENT_ON = true;
// Face count (Section 3): early count of non-triangles.
static bool DOOM_ON = true;

// --- Appendix C (--pruning). ---

// segment pruning (Lemma C.10)
static bool SEGMENTS_CUTOFF = false;
// a face doomed by the pair of its wires
// (doom criteria 4, 5; Lemma C.7)
static bool PAIR_DOOM_ON = false;
// Ablations: --no-segments, --no-table, --table-equal, --no-pair-doom.
static bool NO_SEGMENTS = false;
static bool NO_TABLE = false;
static bool TABLE_EQUAL = false;
static bool NO_PAIR_DOOM = false;

// --- State table (Lemmas C.11, C.12). ---

// 0 none, 1 equal, 2 dominance
static int TABLE_MODE = 0;
// entries (16 bytes each); 2^24 = 256 MiB
static uint64_t TABLE_SIZE = 1u << 24;
// entries per bucket
static const uint64_t TABLE_WAYS = 4;
// memory as given by the user
static std::string PRUNING_ARG;
// --pruning-shm: segment name in /dev/shm
static std::string PRUNING_SHM;
// Other.
static bool FIRST_HIT = false;
static bool QUIET = false;
static bool PRINT_LEDGER = false;
// --stop-on-word: exit code 255 when a word is found
static bool STOP_ON_WORD = false;
// --reduced: place only reduced vertices (Definition 3.8).
static bool REDUCED_ON = false;
static uint64_t reduced_cuts = 0;
// --log: journal of finished chunks
static std::string LOG_FILE;
// --chunks: a list of chunks in one process
static std::string CHUNKS_FILE;
static uint64_t chunks_done = 0;
static uint64_t MAX_NODES = 0;
// b = n(n-2) - 3K, the waste limit
static int WASTE_LIMIT = 0;

// ------------------------------------------------------------- counters ---
static uint64_t visited_nodes = 0;
static uint64_t found = 0;
static uint64_t prefixes = 0;
static uint64_t doom_cuts = 0;
static uint64_t segment_cuts = 0;
static uint64_t table_cuts = 0;
static uint64_t table_probes = 0;
static uint64_t table_hits = 0;
// Checks of the segment-pruning lemmas during the search; they must stay
// zero:
//   endpoint_bad: a double segment with two simple ends (Lemma C.2);
//   legs_bad:     a triple point with > 3 simple legs or with two adjacent
//                 simple legs (Lemma C.4);
//   early_bad:    a segment decided early as "not double" turned out
//                 double (Lemma C.8).
static uint64_t endpoint_bad = 0;
static uint64_t legs_bad = 0;
static uint64_t early_bad = 0;
static volatile sig_atomic_t interrupted = 0;
static std::chrono::steady_clock::time_point start;
static double elapsed()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}
struct Stop {
    const char* reason;
};
// Names for integers.
using Slot = int; // permutation slot 0..n-1; for a letter, the first slot g of its block
using FaceIndex = int; // face c = 0..n (see "state")
using Wire = int; // wire label 0..n-1
using Mult = int; // multiplicity of a letter, m >= 2
using LetterIndex = uint8_t; // position of a letter in the word (e, i); MAXW < 0xFF
using BlockIndex = uint8_t; // index of a wire in the block of a letter before the reversal, 0 at slot g (j); MAXN < 16
using MultiIndex = int; // index of a letter among the letters of multiplicity >= 3 (r)
struct Letter {
    Slot g;
    Mult m;
};
static std::vector<Letter> word;

// ------------------------------------------------------------ state ---
// Face i of the preprint (between slots i and i+1) is stored at c = i+1:
// inner faces are c = 1..n-1, c = 0 and c = n are the unbounded faces at the edges.
// Maximum number of wires; the static_asserts below say what limits it.
static const int MAXN = 14;
// length of a full word = number of pairs
static const int MAXW = MAXN * (MAXN - 1) / 2;
// Table key: the permutation a, 4 bits per slot, then g_prev+1 (0: no letters
// yet) and the required-multiplicity flag. The key layout affects evictions and
// hence the node counts in data/runs: for n <= 12 the shifts 48/52 are kept,
// for larger n they are 4n and 4n+4.
static int KEY_G_PREV_SHIFT = 48;
static int KEY_BIG_SHIFT = 52;
static_assert(4 * MAXN + 5 <= 64, "table key does not fit in 64 bits");
static_assert(MAXN < 16, "slot label and g_prev+1 take 4 bits each; the block index is packed into half a byte");
static_assert(2 * (MAXN - 1) <= 32, "statuses: 2 bits per inner face");
static_assert(MAXN + 1 <= 32, "will_close/two_plus/not_triangle/opened: one bit per face");
static_assert(MAXN <= 16, "seen/alive/paid: one bit per wire");
static_assert(MAXW < 0xFF, "last[]: 0xFF means \"no crossings yet\"");

template <typename W>
struct Bits {
    W v = 0;
    static constexpr W bit(int i) { return W(1) << i; }
    // bits lo..hi inclusive; an empty range (hi < lo) gives 0
    static constexpr W span(int lo, int hi) { return W(((bit(hi) << 1) - 1) & ~(bit(lo) - 1)); }
    void set(int i) { v |= bit(i); }
    void clear(int i) { v &= ~bit(i); }
    void assign(int i, bool b) { b ? set(i) : clear(i); }
    bool test(int i) const { return (v >> i) & 1; }
    void or_mask(W m) { v |= m; }
    void and_mask(W m) { v &= m; }
    void clear_mask(W m) { v &= ~m; }
    int count() const { return __builtin_popcount(v); }
    // Set operations on indices. ~ also sets the bits beyond 0..n.
    friend constexpr Bits operator|(Bits a, Bits b) { return Bits { W(a.v | b.v) }; }
    friend constexpr Bits operator&(Bits a, Bits b) { return Bits { W(a.v & b.v) }; }
    friend constexpr Bits operator~(Bits a) { return Bits { W(~a.v) }; }
    // Index shift: (b >> k).test(i) == b.test(i + k).
    friend constexpr Bits operator>>(Bits a, int k) { return Bits { W(a.v >> k) }; }
    Bits& operator|=(Bits b) { v |= b.v; return *this; }
    Bits& operator&=(Bits b) { v &= b.v; return *this; }
    friend constexpr bool operator==(Bits a, Bits b) { return a.v == b.v; }
    friend constexpr bool operator!=(Bits a, Bits b) { return a.v != b.v; }
    // The set bits in increasing order: for (int i : bits).
    struct Iter {
        W m;
        int operator*() const { return __builtin_ctz(m); }
        Iter& operator++() { m &= W(m - 1); return *this; }
        bool operator!=(const Iter& o) const { return m != o.m; }
    };
    Iter begin() const { return Iter { v }; }
    Iter end() const { return Iter { 0 }; }
};
// Indexed by face c = 0..n.
using FaceBits = Bits<uint32_t>;
// Indexed by wire w = 0..n-1.
using WireBits = Bits<uint16_t>;
// Legs of a triple point in cyclic order (positions 0..5), see leg_after/leg_before.
using LegBits = Bits<uint8_t>;
// Sectors of a vertex (positions 0..2m-1), --reduced.
using SectorBits = Bits<uint8_t>;
// Indexed by letters of multiplicity >= 3 (at most MAXW/3 of them).
using LetterBits = Bits<uint32_t>;
static_assert(sizeof(FaceBits) == 4 && sizeof(WireBits) == 2 && sizeof(LegBits) == 1 && sizeof(SectorBits) == 1 && sizeof(LetterBits) == 4, "each bitmap takes exactly the space of the field it replaces");
// Marks "no crossings yet" in last[].
static constexpr uint8_t NO_LAST = 0xFF;
// Marks "no pending segment" in pending_double.
static constexpr uint8_t NO_END = 0xFF;
// Pending double segment
struct PendingDouble {
    // Ends of the segment
    LetterIndex e1 = NO_END;
    LetterIndex e2 = NO_END;
    // indices of the ends in their blocks, 4 bits each
    BlockIndex j1 : 4;
    BlockIndex j2 : 4;
};
// The last crossing of a wire: vertex e and the index j of the wire in its block;
// e == NO_LAST: no crossings yet.
struct WireCross {
    LetterIndex e = NO_LAST;
    BlockIndex j = 0;
};
// Face status: the number of intermediate vertices since the face opened. The
// values matter: statuses are packed 2 bits each into a table entry and
// compared by "greater" (price_sum).
enum FaceStatus : uint8_t {
    UNOPENED = 0, // not opened by a letter (unbounded on the left)
    NO_MID = 1, // no intermediate vertices
    ONE_MID = 2, // one: if it closes now, the face is a triangle
    TWO_MID = 3, // two or more
};
// Status after one more intermediate vertex.
static inline FaceStatus add_mid(FaceStatus st) { return st == UNOPENED ? UNOPENED : FaceStatus(std::min(3, st + 1)); }

// Segments incident to a vertex (Lemma C.10).
struct VertexSegments {
    uint8_t lambda2 = 0; // lambda2_bar (lambda_bar for multiplicity >= 4)
    uint8_t lambda3 = 0; // lambda3_bar
    uint8_t undecided = 0; // und: undecided segments
    // For a triple point: the positions of the simple legs (D in (C.5)) and of the
    // undecided segments (U), see leg_after/leg_before.
    LegBits simple_legs { };
    LegBits undecided_legs { };
};
// A vertex of multiplicity 3 or 4 under --reduced.
struct ReducedVertex {
    SectorBits nontri { }; // non-triangular sectors
    uint8_t m = 0; // multiplicity
};

// State of a partial word. A segment is the piece of a wire between two of its
// consecutive crossings; the segment of the wire in slot p lies between faces p and p+1.
struct State {
    // --- permutation and faces (Section 3) ---
    std::array<uint8_t, MAXN> a { }; // wire label in each slot
    std::array<FaceStatus, MAXN + 1> status { };
    std::array<uint8_t, MAXN + 1> left_min { }; // min a[0..c-1]
    std::array<uint8_t, MAXN + 1> right_max { }; // max a[c..n-1]
    FaceBits will_close { }; // the face closes in every completion (Lemma 3.6)
    FaceBits two_plus { }; // status == TWO_MID
    // The face, if it closes, is not a triangle: TWO_MID, and with --pruning also
    // doom criteria 4, 5 (Lemma C.7).
    FaceBits not_triangle { };
    FaceBits opened { }; // status != UNOPENED
    FaceBits asc { }; // bit i: a[i] < a[i+1]

    // --- unused-segment count (--pruning) ---
    // pending[c]: finished segments with one face already closed as a non-triangle
    // and the other the open face c. They become unused if c closes as a
    // non-triangle.
    std::array<uint8_t, MAXN + 1> pending { };
    WireBits seen { }; // the wire has already crossed
    int unused = 0; // segments already known to be unused

    // --- segment decisions and vertex charges (--pruning, Lemma C.10) ---
    // Vertex e is the position of its letter in the word.
    std::array<WireCross, MAXN> last { }; // the last crossing of each wire
    std::array<VertexSegments, MAXW> vertex { };
    // pending_double[c]: an undecided segment with one face closed as a triangle and
    // the other the open face c (at most one per face).
    std::array<PendingDouble, MAXN + 1> pending_double { };
    WireBits alive { }; // the current segment of the wire is counted in undecided
    WireBits paid { }; // the current segment of the wire is already counted in unused early
    // r is the index of a letter among the letters of multiplicity >= 3.
    int charges2 = 0; // sum of 2 ch*_lb(v) over the placed vertices
    LetterBits legs_viol { }; // vertex r violates Lemma C.4
    std::array<int16_t, MAXW / 3 + 1> charge2 { }; // 2 ch*_lb of vertex r

    // --- --reduced ---
    // watch[c]: vertices for which the open face c is a sector of a yet unknown
    // type (at most two, see watch_make).
    std::array<std::array<uint8_t, 2>, MAXN + 1> watch { };
    std::array<ReducedVertex, MAXW / 3 + 1> reduced { };
    bool reduced_viol = false;
};
// The top bit of spent marks "the subtree of this state is fully traversed and
// has no words with a3 >= K". Only such entries may cut (Lemma C.12).
static const int CLEAN = 1 << 30;
static_assert(MAXW / 3 + 1 <= 32, "legs_viol: one bit per letter of multiplicity >= 3");
static_assert(std::is_trivially_copyable<State>::value, "State must be trivially copyable");
// Letters of multiplicity >= 3: their positions in word and the inverse map position -> r.
static std::vector<int> multi;
static int mslot[MAXW];
static void push_letter(Slot g, Mult m)
{
    word.push_back({ g, m });
    if (m >= 3) {
        mslot[word.size() - 1] = int(multi.size());
        multi.push_back(int(word.size()) - 1);
    }
}
static void pop_letter()
{
    if (word.back().m >= 3)
        multi.pop_back();
    word.pop_back();
}
// An entry is read and written by one aligned 16-byte operation: on a shared
// table a torn read would join the key of one entry with the fields of another
// and give a false cut. Such access is atomic on CPUs with AVX (Intel SDM, AMD
// APM); how to check the build is in the file header.
struct alignas(16) Entry {
    uint64_t key = 0;
    uint32_t statuses = 0;
    int spent = 0;
};
static_assert(sizeof(Entry) == 16 && alignof(Entry) == 16, "Entry: 16 bytes aligned to 16");
// Bits 20..29 of spent under --reduced: the number of vertices (word length)
// of the entry's prefix (see dominated()).
static const int V_SHIFT = 20;
static int entry_spent(const Entry& e) { return e.spent & ((1 << V_SHIFT) - 1); }
static int entry_vertices(const Entry& e) { return (e.spent & ~CLEAN) >> V_SHIFT; }
static Entry* table = nullptr; // private (mmap) or shared (/dev/shm)
static inline Entry load_entry(const Entry* p)
{
    __m128i v = _mm_load_si128(reinterpret_cast<const __m128i*>(p));
    // Without the barrier the compiler narrows the load to the fields it uses,
    // and one 16-byte load becomes three scalar ones, that is, exactly a torn
    // read. Checked with GCC 13.3 -O2/-O3.
    asm volatile("" : "+x"(v));
    Entry e;
    _mm_store_si128(reinterpret_cast<__m128i*>(&e), v);
    return e;
}
static inline void store_entry(Entry* p, uint64_t key, uint32_t statuses, int spent)
{
    const uint64_t hi = (uint64_t(uint32_t(spent)) << 32) | uint64_t(statuses);
    _mm_store_si128(reinterpret_cast<__m128i*>(p),
        _mm_set_epi64x(int64_t(hi), int64_t(key)));
}

// ------------------------------------------------- table allocation ---
// The private table is an anonymous mmap. The shared one (--pruning-shm) is a
// tmpfs segment or a file on hugetlbfs: the first worker creates it (O_EXCL),
// writes the header and the ready magic last; the others wait for it and
// compare the parameter fingerprint, so that entries of different trees never
// mix. An entry appears only on return from a subtree traversed without words,
// so the segment is valid whatever happens to the run; it is removed by hand.
static const uint64_t SHM_MAGIC = 0x4d554c54454e5532ull; // MULTENU2
static const uint64_t SHM_TABLE_OFF = 4096; // a multiple of the page size and of 16
struct ShmHeader {
    uint64_t magic; // 0 until the segment is ready
    uint64_t table_size;
    uint64_t total_bytes;
    uint32_t table_ways;
    uint32_t layers;
    int32_t n;
    int32_t target;
    int32_t budget;
    int32_t max_mult;
    int32_t require_mult;
};
static void table_alloc();

// ---------------------------------------------- table instrumentation ---
// --reuse-stats: the reuse distance is the number of distinct keys between two
// accesses to the same key. The share of reuses with distance < C is the hit
// rate of an ideal LRU cache of C entries. Does not affect the search.
static bool REUSE_STATS = false;
static std::unordered_map<uint64_t, uint64_t> reuse_last; // key -> time
static std::vector<uint8_t> reuse_live; // 1 if this is the last access to its key
static std::vector<int32_t> reuse_bit; // Fenwick tree over reuse_live (1-based)
static uint64_t reuse_t = 0;
static uint64_t reuse_first = 0;
static uint64_t reuse_hist[66] = { 0 };
static uint64_t evictions = 0; // an insertion over a DIFFERENT key
static void bit_add(size_t i, int v)
{
    for (size_t k = i + 1; k < reuse_bit.size(); k += k & (~k + 1))
        reuse_bit[k] += v;
}
static uint64_t bit_sum(size_t i)
{ // sum over [0, i]
    uint64_t r = 0;
    for (size_t k = i + 1; k > 0; k -= k & (~k + 1))
        r += uint64_t(reuse_bit[k]);
    return r;
}
static void reuse_grow(size_t need)
{
    if (need < reuse_live.size())
        return;
    size_t cap = std::max<size_t>(size_t(1) << 20, reuse_live.size() * 2);
    while (cap <= need)
        cap *= 2;
    reuse_live.resize(cap, 0);
    reuse_bit.assign(cap + 1, 0); // linear rebuild of the Fenwick tree
    for (size_t i = 0; i < cap; ++i)
        reuse_bit[i + 1] += reuse_live[i];
    for (size_t k = 1; k <= cap; ++k) {
        size_t j = k + (k & (~k + 1));
        if (j <= cap)
            reuse_bit[j] += reuse_bit[k];
    }
}
// Final report: the curve "capacity -> hit rate of an ideal LRU", the
// breakdown of misses and the fill of the real table.
static std::string reuse_report();
static void reuse_note(uint64_t key)
{
    auto it = reuse_last.find(key);
    if (it == reuse_last.end())
        ++reuse_first;
    else {
        uint64_t t0 = it->second;
        uint64_t d = bit_sum(reuse_t - 1) - bit_sum(t0); // distinct keys in (t0, now)
        ++reuse_hist[d ? 64 - __builtin_clzll(d) : 0];
        bit_add(t0, -1);
        reuse_live[t0] = 0;
    }
    reuse_grow(reuse_t);
    reuse_live[reuse_t] = 1;
    bit_add(reuse_t, 1);
    reuse_last[key] = reuse_t;
    ++reuse_t;
}
static int pairs(int m) { return m * (m - 1) / 2; }
// The legs (segments) of a triple point take positions r = 0..5 around it;
// adjacent positions bound a sector, r and r+3 lie on one wire. The functions
// take the index j of the wire in the block and return the bit of its position:
// the segment after the vertex has r = 2 - j (0..2), before it r = 5 - j (3..5).
static inline LegBits leg_after(BlockIndex j) { return LegBits { LegBits::bit(2 - j) }; }
static inline LegBits leg_before(BlockIndex j) { return LegBits { LegBits::bit(5 - j) }; }
// Leg positions as the vertices of the cycle C6 (adjacent positions are adjacent).
// LEGS_INDEPENDENT[D]: D is independent (no two adjacent positions);
// MAX_INDEPENDENT_ADD[D][U]: the largest number of positions from U that can be
// added to D keeping it independent.
static uint8_t MAX_INDEPENDENT_ADD[64][64];
static bool LEGS_INDEPENDENT[64];
static void init_leg_tables()
{
    for (int D = 0; D < 64; ++D) {
        const LegBits d { uint8_t(D) };
        LEGS_INDEPENDENT[D] = true;
        for (int r = 0; r < 6; ++r)
            if (d.test(r) && d.test((r + 1) % 6)) // adjacent legs
                LEGS_INDEPENDENT[D] = false;
    }
    for (int D = 0; D < 64; ++D)
        for (int U = 0; U < 64; ++U) {
            int best = 0;
            // I runs over all subsets of U, from U down to the empty set
            for (int I = U;; I = (I - 1) & U) {
                if (LEGS_INDEPENDENT[D | I])
                    best = std::max(best, LegBits { uint8_t(I) }.count());
                if (!I)
                    break;
            }
            MAX_INDEPENDENT_ADD[D][U] = uint8_t(best);
        }
}
static int tri_loss(int m) { return (m - 1) * (m - 2) / 2; }

static void set_status(State& s, FaceIndex c, FaceStatus v)
{
    s.status[c] = v;
    s.opened.assign(c, v != UNOPENED);
    if (c > 0 && c < N)
        s.two_plus.assign(c, v == TWO_MID);
}
// Face c gets an intermediate vertex: the block of the letter starts at slot c
// or ends at slot c-1.
static void intermediate(State& s, FaceIndex c)
{
    if (s.status[c] != UNOPENED)
        set_status(s, c, add_mid(s.status[c]));
}
// A doomed face can no longer become a bounded triangle (doom criteria 1-5,
// Appendix C, "Doomed faces"); criteria 4, 5 are part of not_triangle.
static inline FaceBits doomed_mask(const State& s) { return ~s.opened | ~s.will_close | s.not_triangle; }
static inline bool doomed(const State& s, FaceIndex c) { return doomed_mask(s).test(c); }
// Update of not_triangle after the letter (g, m): criteria 4, 5 by the pair of
// wires of a face (Lemma C.7) change only for the faces g..g+m.
static void update_not_triangle(State& s, Slot g, Mult m)
{
    if (!PAIR_DOOM_ON) {
        s.not_triangle = s.two_plus;
        return;
    }
    const FaceIndex lo = std::max(g, 1);
    const FaceIndex hi = std::min(g + m, N - 1);
    FaceBits bits { };
    for (FaceIndex c = lo; c <= hi; ++c)
        if (s.a[c - 1] > s.a[c] && (s.status[c] == ONE_MID || (s.status[c] == NO_MID && s.a[c - 1] > s.right_max[c] && s.a[c] < s.left_min[c])))
            bits.set(c);
    s.not_triangle.clear_mask(FaceBits::span(lo, hi)); // faces changed by this letter
    s.not_triangle |= bits | s.two_plus;
}

// ------------------------------------------------------ segment count ---
// Vertices whose segments the current letter touched; their charges are
// recomputed at the end of apply (repeats are harmless). A letter of
// multiplicity m makes at most 7m+5 touches, hence the array size.
static LetterIndex touched[8 * MAXN + 8];
static int ntouched = 0;
static inline void touch(LetterIndex e) { touched[ntouched++] = e; }
// A letter ends the current segments of the wires of its block. For the middle
// wires both faces close with this letter, and the segment goes to unused at
// once if neither is a triangle. For the outer wires the outer face (g or g+m)
// stays open: if the inner face closed as a non-triangle, the segment waits in
// pending. The inner faces that close resolve their pending.
static void segments_apply(State& s, Slot g, Mult m)
{
    for (BlockIndex j = 0; j < m; ++j) {
        Slot p = g + j;
        Wire w = s.a[p];
        if (!s.seen.test(w))
            continue;
        if (s.paid.test(w)) {
            s.paid.clear(w);
            continue;
        }
        bool upper_tri = s.status[p] == ONE_MID;
        bool lower_tri = s.status[p + 1] == ONE_MID;
        if (j == 0) {
            if (!lower_tri)
                ++s.pending[g];
        } else if (j == m - 1) {
            if (!upper_tri)
                ++s.pending[g + m];
        } else if (!upper_tri && !lower_tri)
            ++s.unused;
    }
    for (FaceIndex c = g + 1; c < g + m; ++c) {
        if (s.status[c] != ONE_MID)
            s.unused += s.pending[c];
        s.pending[c] = 0;
    }
    for (Slot p = g; p < g + m; ++p)
        s.seen.set(s.a[p]);
}
// A segment decided as double, between vertices e1 and e2 (the segment after e1
// of wire j1 and before e2 of wire j2), passes its charge by the rule before (C.5):
// to a vertex of multiplicity >= 4, to the earlier one if there are two (Lemma C.3);
// half each to two triple points (lambda3); to the triple point if the other end
// is simple, as its simple leg (lambda2). Two simple ends are excluded by Lemma C.2.
static void charge(State& s, LetterIndex e1, Mult m1, BlockIndex j1, LetterIndex e2, Mult m2, BlockIndex j2)
{
    touch(e1);
    touch(e2);
    if (m1 >= 4 || m2 >= 4)
        ++s.vertex[m1 >= 4 ? e1 : e2].lambda2;
    else if (m1 == 3 && m2 == 3) {
        ++s.vertex[e1].lambda3;
        ++s.vertex[e2].lambda3;
    } else if (m1 == 3) {
        ++s.vertex[e1].lambda2;
        s.vertex[e1].simple_legs |= leg_after(j1);
    } else if (m2 == 3) {
        ++s.vertex[e2].lambda2;
        s.vertex[e2].simple_legs |= leg_before(j2);
    } else
        ++endpoint_bad;
}
// The segment is decided: remove it from undecided of vertex e1 (the segment
// after it) or e2 (the segment before it).
static void drop_undecided_after(State& s, LetterIndex e1, BlockIndex j1)
{
    touch(e1);
    --s.vertex[e1].undecided;
    if (word[e1].m == 3)
        s.vertex[e1].undecided_legs &= ~leg_after(j1);
}
static void drop_undecided_before(State& s, LetterIndex e2, BlockIndex j2)
{
    touch(e2);
    --s.vertex[e2].undecided;
    if (word[e2].m == 3)
        s.vertex[e2].undecided_legs &= ~leg_before(j2);
}
// The pending segment of face c is double if c closed as a triangle.
static void resolve_pending_double(State& s, FaceIndex c, bool is_double)
{
    const PendingDouble p = s.pending_double[c];
    drop_undecided_after(s, p.e1, p.j1);
    drop_undecided_before(s, p.e2, p.j2);
    if (is_double)
        charge(s, p.e1, word[p.e1].m, p.j1, p.e2, word[p.e2].m, p.j2);
    s.pending_double[c] = { NO_END, NO_END, 0, 0 };
}
// Letter e = (g, m), before the block reversal: decides the segments it ends.
// Both faces triangles: double; at least one is not: not double. For an outer
// wire the outer face is still open; if the inner face is a triangle and the
// outer may still become one, the segment is put into pending_double.
// Then the pending_double of the faces closed by this letter and of the outer
// faces getting a second intermediate vertex are decided.
static void resolve_segments_before(State& s, Slot g, Mult m, LetterIndex e)
{
    for (BlockIndex j = 0; j < m; ++j) {
        Slot p = g + j;
        Wire w = s.a[p];
        const WireCross prev = s.last[w];
        if (prev.e == NO_LAST)
            continue;
        bool upper_tri = s.status[p] == ONE_MID;
        bool lower_tri = s.status[p + 1] == ONE_MID;
        if (!s.alive.test(w)) { // early decision "not double" (Lemma C.8): check it
            if (j > 0 && j < m - 1) {
                if (upper_tri && lower_tri)
                    ++early_bad;
            } else if ((j == 0 ? lower_tri : upper_tri) && !doomed(s, j == 0 ? g : g + m))
                ++early_bad;
            continue;
        }
        s.alive.clear(w);
        Mult m1 = word[prev.e].m;
        if (j > 0 && j < m - 1) {
            drop_undecided_after(s, prev.e, prev.j);
            if (upper_tri && lower_tri)
                charge(s, prev.e, m1, prev.j, e, m, j);
            continue;
        }
        bool closed_tri = (j == 0) ? lower_tri : upper_tri;
        FaceIndex outer_face = (j == 0) ? g : g + m;
        if (!closed_tri) {
            drop_undecided_after(s, prev.e, prev.j);
            continue;
        }
        // The outer face gets an intermediate vertex; if the pair of its wires
        // descends, it is doomed (Lemma C.7(a)), and the segment will not be double.
        bool doomed_after = (outer_face == g) ? (g > 0 && s.a[g - 1] > s.a[g + m - 1])
                                              : (g + m < N && s.a[g] > s.a[g + m]);
        if (s.status[outer_face] == NO_MID && s.will_close.test(outer_face) && !doomed_after) {
            s.pending_double[outer_face] = { prev.e, e, prev.j, j };
            ++s.vertex[e].undecided;
            if (m == 3)
                s.vertex[e].undecided_legs |= leg_before(j);
        } else
            drop_undecided_after(s, prev.e, prev.j);
    }
    for (FaceIndex inner_face = g + 1; inner_face < g + m; ++inner_face)
        if (s.pending_double[inner_face].e1 != NO_END)
            resolve_pending_double(s, inner_face, s.status[inner_face] == ONE_MID);
    for (FaceIndex outer_face : { g, g + m }) {
        if (s.status[outer_face] != ONE_MID)
            continue; // becomes TWO_MID now
        if (s.pending_double[outer_face].e1 != NO_END)
            resolve_pending_double(s, outer_face, false);
        Slot q = (outer_face == g) ? g - 1 : g + m;
        if (q < 0 || q >= N)
            continue;
        Wire w = s.a[q];
        if (s.alive.test(w)) {
            s.alive.clear(w);
            drop_undecided_after(s, s.last[w].e, s.last[w].j);
        }
    }
}
// The wire in slot p will cross again: there is a pair it has not crossed yet
// (a smaller label to the left or a larger one to the right).
static bool has_future_event(const State& s, Slot p)
{
    return s.left_min[p] < s.a[p] || s.right_max[p + 1] > s.a[p];
}
// Early count of the current segment of the wire in slot p (Lemma C.8(b)): it is
// bounded and both its faces are doomed, so it goes to unused at once.
static void pay_open_segment(State& s, Slot p)
{
    Wire w = s.a[p];
    if (!s.seen.test(w) || s.paid.test(w))
        return;
    if (has_future_event(s, p) && doomed(s, p) && doomed(s, p + 1)) {
        ++s.unused;
        s.paid.set(w);
    }
}
// Letter e = (g, m), after the block reversal: starts new segments of the wires
// of the block (undecided unless one of their faces is doomed) and counts in
// unused early the segments at doomed faces.
static void resolve_segments_after(State& s, Slot g, Mult m, LetterIndex e)
{
    for (Slot p = g; p < g + m; ++p) {
        Wire w = s.a[p];
        BlockIndex j = g + m - 1 - p; // index of the wire in the block before the reversal
        s.last[w] = { e, j };
        if (!(doomed(s, p) || doomed(s, p + 1))) {
            s.alive.set(w);
            ++s.vertex[e].undecided;
            if (m == 3)
                s.vertex[e].undecided_legs |= leg_after(j);
        }
    }
    for (Slot p = g; p < g + m; ++p)
        pay_open_segment(s, p);
    if (g > 0)
        pay_open_segment(s, g - 1);
    if (g + m < N)
        pay_open_segment(s, g + m);
    // Neighbors of the block: face g or g+m may have become doomed, and then their
    // current segment is not double.
    for (Slot q : { g - 1, g + m }) {
        if (q < 0 || q >= N)
            continue;
        Wire w = s.a[q];
        if (s.alive.test(w) && (doomed(s, q) || doomed(s, q + 1))) {
            s.alive.clear(w);
            drop_undecided_after(s, s.last[w].e, s.last[w].j);
        }
    }
}
// 2 ch*_lb(v) of vertex i of multiplicity k >= 3 by (C.5); viol: Lemma C.4 is
// violated (lambda2_bar > 3 or two adjacent simple legs).
static inline int charge_lb2(const State& s, LetterIndex i, Mult k, bool& viol)
{
    const VertexSegments& v = s.vertex[i];
    int lambda2 = v.lambda2;
    int lambda3 = v.lambda3;
    int undecided = v.undecided;
    viol = false;
    if (k == 3) {
        viol = lambda2 > 3 || !LEGS_INDEPENDENT[v.simple_legs.v];
        int xi = MAX_INDEPENDENT_ADD[v.simple_legs.v][v.undecided_legs.v];
        return 6 - (2 * lambda2 + lambda3 + undecided + xi);
    }
    return 2 * (k * (k - 2) - lambda2 - undecided);
}
// Recompute the charges of the touched vertices; e, m is the current letter (not yet in word).
static void charges_update(State& s, LetterIndex e, Mult m)
{
    if (m >= 3)
        touch(e);
    for (int t = 0; t < ntouched; ++t) {
        LetterIndex i = touched[t];
        Mult k = i == e ? m : word[i].m;
        if (k < 3)
            continue;
        MultiIndex r = i == e ? int(multi.size()) : mslot[i];
        bool viol;
        int ch = charge_lb2(s, i, k, viol);
        s.charges2 += ch - s.charge2[r];
        s.charge2[r] = int16_t(ch);
        s.legs_viol.assign(r, viol);
    }
    ntouched = 0;
}
static int charges_sum(const State& s)
{
    legs_bad += s.legs_viol.count(); // once per violating letter, as in a full pass
    return s.charges2;
}

// ------------------------------------------------------- --reduced ---
// The sectors of a vertex of multiplicity m take positions 0..2m-1 around it (see apply).
// A reduced vertex (Definition 3.8): a triple point has at most one non-triangle
// in each parity class of sectors; a fourfold point has at most two, and two
// only if they are opposite. Multiplicities >= 5 are not placed.
// Parity classes of the sectors of a triple point: even (0,2,4) and odd (1,3,5).
static constexpr SectorBits EVEN_SECTORS { 0x15 };
static constexpr SectorBits ODD_SECTORS { 0x2A };
// Sectors 0..3 of a fourfold point; sector p is opposite to p+4.
static constexpr SectorBits LOW_SECTORS { 0x0F };
// Watcher code in watch[c]: (r*8 + sector position) + 1; 0 means no watcher.
static constexpr uint8_t watch_make(MultiIndex r, int pos) {
    return uint8_t(8 * r + pos + 1);
}
static constexpr MultiIndex watch_r(uint8_t w) {
    return (w - 1) >> 3;
}
static constexpr int watch_pos(uint8_t w) {
    return (w - 1) & 7;
}
// A vertex of multiplicity m with these non-triangular sectors is not reduced.
// Two opposite sectors of a fourfold point are {p, p+4}, p < 4.
static inline bool reduced_bad(Mult m, SectorBits nontri_sectors)
{
    if (m == 3)
        return (nontri_sectors & EVEN_SECTORS).count() >= 2 || (nontri_sectors & ODD_SECTORS).count() >= 2;
    int n_nontri = nontri_sectors.count();
    return n_nontri >= 3 || (n_nontri == 2 && (nontri_sectors >> 4) != (nontri_sectors & LOW_SECTORS));
}
// The sector watched by w turned out not to be a triangle.
static inline void reduced_nontri(State& s, uint8_t w)
{
    MultiIndex r = watch_r(w);
    int pos = watch_pos(w);
    s.reduced[r].nontri.set(pos);
    if (reduced_bad(s.reduced[r].m, s.reduced[r].nontri))
        s.reduced_viol = true;
}
// The type of face c became known (tri: a triangle); its watchers are removed.
static inline void reduced_resolve(State& s, FaceIndex c, bool tri)
{
    for (int i = 0; i < 2; ++i) {
        uint8_t w = s.watch[c][i];
        if (!w)
            continue;
        if (!tri)
            reduced_nontri(s, w);
        s.watch[c][i] = 0;
    }
}
static inline void reduced_attach(State& s, FaceIndex c, uint8_t w)
{
    if (!s.watch[c][0])
        s.watch[c][0] = w;
    else if (!s.watch[c][1])
        s.watch[c][1] = w;
    else
        throw std::runtime_error("--reduced: third watcher on a face");
}

// ----------------------------------------------------------- letter ---
// Places the letter e = (g, m): reverses the block of slots g..g+m-1, closes the
// inner faces of the block and opens them again. Returns the number of closed
// bounded non-triangular faces.
static int apply(State& s, Slot g, Mult m, LetterIndex e)
{
    int nontri = 0;
    ntouched = 0;
    Bits<uint32_t> reduced_left { }; // j: the closed left face g+1+j is not a triangle
    if (REDUCED_ON) {
        if (m == 3 || m == 4)
            for (int j = 0; j < m - 1; ++j)
                if (s.status[g + 1 + j] != ONE_MID)
                    reduced_left.set(j);
        for (FaceIndex c = g + 1; c < g + m; ++c)
            reduced_resolve(s, c, s.status[c] == ONE_MID);
    }
    if (SEGMENTS_CUTOFF) {
        segments_apply(s, g, m);
        resolve_segments_before(s, g, m, e);
    }
    for (FaceIndex c = g + 1; c < g + m; ++c) {
        nontri += s.status[c] == NO_MID || s.status[c] == TWO_MID; // bounded, not a triangle
        s.status[c] = NO_MID;
    }
    {
        const uint32_t inner = FaceBits::span(g + 1, g + m - 1); // faces g+1..g+m-1
        s.opened.or_mask(inner);
        s.two_plus.clear_mask(inner);
    }
    intermediate(s, g);
    intermediate(s, g + m);
    std::reverse(s.a.begin() + g, s.a.begin() + g + m);
    for (int i = std::max(g - 1, 0); i < g + m && i + 1 < N; ++i)
        s.asc.assign(i, s.a[i] < s.a[i + 1]);
    for (FaceIndex c = g + 1; c < g + m; ++c) {
        s.left_min[c] = std::min(s.left_min[c - 1], s.a[c - 1]);
        s.will_close.assign(c, s.left_min[c] < N - c);
    }
    for (FaceIndex c = g + m - 1; c > g; --c)
        s.right_max[c] = std::max(s.right_max[c + 1], s.a[c]);
    update_not_triangle(s, g, m);
    if (REDUCED_ON) {
        // A doomed face does not close as a triangle.
        for (FaceIndex c = g; c <= g + m; ++c)
            if ((s.watch[c][0] || s.watch[c][1]) && doomed(s, c))
                reduced_resolve(s, c, false);
        if (m == 3 || m == 4) {
            // Sectors of the new vertex in order: 0 the top (face g), 1..m-1 the closed
            // left faces g+1..g+m-1 from top to bottom, m the bottom (face g+m), m+1..2m-1
            // the new right faces g+m-1..g+1 from bottom to top; p is opposite to p+m.
            // The types of the open sectors become known later; watch tracks them.
            MultiIndex r = int(multi.size());
            s.reduced[r].m = uint8_t(m);
            SectorBits nontri_sectors { };
            for (int j = 0; j < m - 1; ++j)
                if (reduced_left.test(j))
                    nontri_sectors.set(1 + j);
            auto side = [&](FaceIndex c, int pos) {
                if (doomed(s, c))
                    nontri_sectors.set(pos);
                else
                    reduced_attach(s, c, watch_make(r, pos));
            };
            side(g, 0);
            side(g + m, m);
            for (int j = 0; j < m - 1; ++j)
                side(g + m - 1 - j, m + 1 + j); // right ones from bottom to top
            s.reduced[r].nontri = nontri_sectors;
            if (reduced_bad(m, nontri_sectors))
                s.reduced_viol = true;
        }
    }
    if (SEGMENTS_CUTOFF) {
        resolve_segments_after(s, g, m, e);
        charges_update(s, e, m);
    }
    return nontri;
}
// Forced descent (Lemma 3.7): the suffix-maximal wire in slot g_prev that has
// not reached its final slot descends with the next letter.
static bool is_forced_descend(const State& s, Slot g_prev)
{
    if (!FORCED_DESCENT_ON || g_prev < 0 || g_prev <= N - 1 - s.a[g_prev])
        return false;
    for (int i = g_prev + 1; i < N; ++i)
        if (s.a[i] > s.a[g_prev])
            return false;
    return true;
}
// Is the letter (g, m) allowed after a letter starting at g_prev? Same rules as
// in search: the block ascends, canonical form (Lemma 3.1), forced descent,
// --first-fixed. Used to replay chunks.
static bool legal(const State& s, Slot g_prev, Slot g, Mult m, bool forced)
{
    if (m < 2 || m > MAX_MULT || g < 0 || g + m > N)
        return false;
    if (g_prev < 0) {
        if (FIRST_FIXED && g > 0)
            return false;
    } else if (g < g_prev - m + 1)
        return false;
    if (forced && g != g_prev - m + 1)
        return false;
    for (int i = g; i + 1 < g + m; ++i)
        if (s.a[i] > s.a[i + 1])
            return false;
    return true;
}

// ---------------------------------------------------- state table ---
static uint64_t mix(uint64_t x)
{
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}
static inline uint64_t table_key(uint64_t packed, Slot g_prev, bool has_required)
{
    return packed | (uint64_t(g_prev + 1) << KEY_G_PREV_SHIFT) | (uint64_t(has_required) << KEY_BIG_SHIFT);
}
// The permutation a, 4 bits per slot.
static uint64_t pack_wires(const State& s)
{
    uint64_t p = 0;
    for (int i = 0; i < N; ++i)
        p |= uint64_t(s.a[i]) << (4 * i);
    return p;
}
// Statuses of the inner faces 1..n-1, 2 bits each.
static uint32_t pack_statuses(const State& s)
{
    uint32_t r = 0;
    for (FaceIndex c = 1; c < N; ++c)
        r |= uint32_t(s.status[c]) << (2 * (c - 1));
    return r;
}
static inline Entry* bucket(uint64_t key)
{
    return &table[(mix(key) & (TABLE_SIZE / TABLE_WAYS - 1)) * TABLE_WAYS];
}
// The price of face c (C.7) when status_X(c) > status_Y(c): 0 if the order of
// the pair of its wires rules out a loss, 1 otherwise.
static inline int face_price(const State& s, FaceIndex c, unsigned ax, unsigned ay, const uint8_t* right_max)
{
    if (s.a[c - 1] <= s.a[c])
        return 1;
    if (s.a[c - 1] > right_max[c] && s.a[c] < s.left_min[c])
        return 0;
    return (ax == TWO_MID && ay == ONE_MID) ? 0 : 1;
}
// Sum of the prices of the faces where the status of entry est exceeds the
// current cur; counts until it exceeds limit.
static int price_sum(const State& s, uint32_t est, uint32_t cur, int limit, const uint8_t* right_max)
{
    int d = 0;
    for (FaceIndex c : s.will_close) { // the other faces will not close any more
        if (d > limit)
            break;
        unsigned ax = (est >> (2 * (c - 1))) & 3u;
        unsigned ay = (cur >> (2 * (c - 1))) & 3u;
        if (ax > ay)
            d += face_price(s, c, ax, ay, right_max);
    }
    return d;
}
// Does a table entry with the same key cut the node: equal statuses and no
// larger spending (--table-equal) or dominance (Lemma C.11).
static bool dominated(const State& s, int spent, uint64_t key, uint32_t statuses)
{
    ++table_probes;
    if (REUSE_STATS)
        reuse_note(key);
    Entry* b = bucket(key);
    for (uint64_t w = 0; w < TABLE_WAYS; ++w) {
        const Entry e = load_entry(&b[w]);
        if (e.key != key)
            continue;
        ++table_hits;
        // Only CLEAN entries cut: the subtree was traversed without words (Lemma C.12).
        if (!(e.spent & CLEAN))
            continue;
        // --reduced: only an entry with at least as many vertices (Lemma C.14).
        if (REDUCED_ON && entry_vertices(e) < int(word.size()))
            continue;
        if (TABLE_MODE == 1) {
            if (e.statuses == statuses && entry_spent(e) <= spent) {
                ++table_cuts;
                return true;
            }
        } else if (entry_spent(e) + price_sum(s, e.statuses, statuses, spent - entry_spent(e), s.right_max.data()) <= spent) {
            ++table_cuts;
            return true;
        }
    }
    return false;
}

static std::string reuse_report()
{
    char b[4096];
    int p = 0;
    uint64_t rep = 0;
    for (int k = 0; k < 66; ++k)
        rep += reuse_hist[k];
    p += snprintf(b + p, sizeof b - p,
        "\n  reuse: probes %llu, first-time %llu (%.1f%%), repeats %llu (%.1f%%)",
        (unsigned long long)reuse_t, (unsigned long long)reuse_first,
        100.0 * double(reuse_first) / double(reuse_t ? reuse_t : 1),
        (unsigned long long)rep, 100.0 * double(rep) / double(reuse_t ? reuse_t : 1));
    p += snprintf(b + p, sizeof b - p, "\n  ideal LRU, share of ALL probes that would hit at capacity:");
    uint64_t acc = 0;
    for (int k = 0; k <= 28; ++k) { // hist[k]: distances [2^(k-1), 2^k)
        acc += reuse_hist[k];
        if (k >= 10 && k % 2 == 0)
            p += snprintf(b + p, sizeof b - p, "  2^%d=%.1f%%", k, 100.0 * double(acc) / double(reuse_t ? reuse_t : 1));
    }
    // the real table: fill, entries usable for cuts, duplicates in a bucket
    uint64_t used = 0;
    uint64_t clean = 0;
    uint64_t dup = 0;
    for (uint64_t base = 0; base + TABLE_WAYS <= TABLE_SIZE; base += TABLE_WAYS) {
        for (uint64_t w = 0; w < TABLE_WAYS; ++w) {
            if (!table[base + w].key)
                continue;
            ++used;
            if (table[base + w].spent & CLEAN)
                ++clean;
            for (uint64_t v = 0; v < w; ++v)
                if (table[base + v].key == table[base + w].key) {
                    ++dup;
                    break;
                }
        }
    }
    p += snprintf(b + p, sizeof b - p,
        "\n  table: %.1f%% of cells used, of them CLEAN %.1f%%, duplicate keys %.1f%%; evictions %llu",
        100.0 * double(used) / double(TABLE_SIZE ? TABLE_SIZE : 1),
        100.0 * double(clean) / double(used ? used : 1),
        100.0 * double(dup) / double(used ? used : 1), (unsigned long long)evictions);
    return std::string(b, size_t(p));
}

// Fingerprint of the enabled prunings: workers with different sets build
// different trees, and their entries are incompatible. FIRST_FIXED is a precaution.
static uint32_t layers_code()
{
    return 1 | uint32_t(FIRST_FIXED) << 1 | uint32_t(FORCED_DESCENT_ON) << 2 | uint32_t(DOOM_ON) << 3 | uint32_t(SEGMENTS_CUTOFF) << 4 | uint32_t(PAIR_DOOM_ON) << 5 | uint32_t(TABLE_MODE) << 6 | uint32_t(REDUCED_ON) << 8;
}
static void sleep_ms(long ms)
{
    struct timespec t { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&t, nullptr);
}
static void table_alloc()
{
    const size_t bytes = size_t(TABLE_SIZE) * sizeof(Entry);
    if (PRUNING_SHM.empty()) {
        void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED)
            throw std::runtime_error(std::string("mmap: ") + strerror(errno));
        // The kernel gives huge pages (THP) on first touch; with random touches the
        // table would be built of normal pages, so it is zeroed sequentially right
        // away.
        madvise(p, bytes, MADV_HUGEPAGE);
        memset(p, 0, bytes);
        table = static_cast<Entry*>(p);
        return;
    }
    ShmHeader want { };
    want.table_size = TABLE_SIZE;
    want.total_bytes = SHM_TABLE_OFF + bytes;
    want.table_ways = uint32_t(TABLE_WAYS);
    want.layers = layers_code();
    want.n = N;
    want.target = TARGET_FACES;
    want.budget = BUDGET_FACES;
    want.max_mult = MAX_MULT;
    want.require_mult = REQUIRE_MULT;

    // A name without a slash is a POSIX shm segment in /dev/shm; a path is a
    // plain file, e.g. on hugetlbfs (2 MiB pages).
    std::string name = PRUNING_SHM;
    const bool as_path = name.find('/', 1) != std::string::npos; // a slash inside the name
    if (!as_path && name[0] != '/')
        name = "/" + name;
    // hugetlbfs requires both the file size and the mapping length to be multiples
    // of the huge page size; the header and the tail are aligned to 2 MiB.
    if (as_path) {
        const uint64_t HUGE = 2u << 20;
        want.total_bytes = ((want.total_bytes + HUGE - 1) / HUGE) * HUGE;
    }
    bool creator = true;
    int fd = as_path ? open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600)
                     : shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0 && errno == EEXIST) {
        creator = false;
        fd = as_path ? open(name.c_str(), O_RDWR) : shm_open(name.c_str(), O_RDWR, 0600);
    }
    if (fd < 0)
        throw std::runtime_error("open " + name + ": " + strerror(errno));
    if (creator && ftruncate(fd, off_t(want.total_bytes)) != 0) {
        int e = errno;
        if (as_path)
            unlink(name.c_str());
        else
            shm_unlink(name.c_str());
        throw std::runtime_error(std::string("ftruncate ") + name + ": " + strerror(e));
    }
    if (!creator) { // only a full-size file can be mapped
        struct stat st { };
        for (int i = 0; i < 60000; ++i) {
            if (fstat(fd, &st) != 0)
                throw std::runtime_error(std::string("fstat: ") + strerror(errno));
            if (uint64_t(st.st_size) >= want.total_bytes)
                break;
            sleep_ms(1);
        }
        if (uint64_t(st.st_size) < want.total_bytes)
            throw std::runtime_error("shm " + name + ": segment is smaller than requested"
                                                     " (a creator that died, or another --pruning size)");
    }
    void* p = mmap(nullptr, want.total_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int map_err = errno;
    close(fd);
    if (p == MAP_FAILED)
        throw std::runtime_error(std::string("mmap shm: ") + strerror(map_err));
    ShmHeader* h = static_cast<ShmHeader*>(p);
    if (creator) {
        *h = want; // magic in want is zero
        __atomic_store_n(&h->magic, SHM_MAGIC, __ATOMIC_RELEASE);
    } else {
        int i = 0;
        while (__atomic_load_n(&h->magic, __ATOMIC_ACQUIRE) != SHM_MAGIC) {
            if (++i > 60000)
                throw std::runtime_error("shm " + name + ": no ready mark");
            sleep_ms(1);
        }
        if (h->table_size != want.table_size || h->total_bytes != want.total_bytes || h->table_ways != want.table_ways || h->layers != want.layers || h->n != want.n || h->target != want.target || h->budget != want.budget || h->max_mult != want.max_mult || h->require_mult != want.require_mult)
            throw std::runtime_error("shm " + name + ": parameters differ from the segment's");
    }
    table = reinterpret_cast<Entry*>(static_cast<char*>(p) + SHM_TABLE_OFF);
}

// Chunk journal: chunk, exit code, nodes, words. A line is written for an
// interrupted chunk too (code 2); only code 0 counts as done. A line shorter
// than PIPE_BUF is appended atomically with O_APPEND, so workers share one file.
static void log_chunk(const std::string& c, int code, uint64_t nd, uint64_t wd)
{
    if (LOG_FILE.empty())
        return;
    char line[PIPE_BUF];
    int len = snprintf(line, sizeof line, "%s\t%d\t%llu\t%llu\n", c.c_str(), code,
        (unsigned long long)nd, (unsigned long long)wd);
    if (len <= 0 || len >= int(sizeof line)) {
        fprintf(stderr, "WARNING log line for chunk %s exceeds PIPE_BUF, not logged\n", c.c_str());
        return;
    }
    int fd = open(LOG_FILE.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        fprintf(stderr, "WARNING cannot open log %s: %s\n", LOG_FILE.c_str(), strerror(errno));
        return;
    }
    if (write(fd, line, size_t(len)) != len)
        fprintf(stderr, "WARNING short write to log %s\n", LOG_FILE.c_str());
    close(fd);
}

// ------------------------------------------------------------- output ---
static const char* zone_str()
{
    static char z[64];
    int p = 0;
    if (REQUIRE_MULT >= 3)
        p += snprintf(z + p, sizeof z - p, "m>=%d", REQUIRE_MULT);
    if (MAX_MULT < N)
        p += snprintf(z + p, sizeof z - p, "%sm<=%d", p ? "," : "", MAX_MULT);
    if (!p)
        snprintf(z, sizeof z, "all");
    return z;
}
static const char* flags_str()
{
    static char f[128];
    int p = 0;
    auto add = [&](const char* t) { p += snprintf(f + p, sizeof f - p, "%s%s", p ? "," : "", t); };
    if (SEGMENTS_CUTOFF || TABLE_MODE || PAIR_DOOM_ON) {
        char b[48];
        snprintf(b, sizeof b, "pruning=%s", PRUNING_ARG.c_str());
        add(b);
        if (!SEGMENTS_CUTOFF)
            add("no-segments");
        if (!TABLE_MODE)
            add("no-table");
        if (!PAIR_DOOM_ON)
            add("no-pair-doom");
        if (TABLE_MODE == 1)
            add("table-equal");
    }
    if (FIRST_FIXED)
        add("first-fixed");
    if (!FORCED_DESCENT_ON)
        add("no-forced-descent");
    if (!DOOM_ON)
        add("no-doom");
    if (REDUCED_ON)
        add("reduced");
    if (!p)
        snprintf(f, sizeof f, "default");
    return f;
}
static void fput_word(FILE* out)
{
    for (size_t i = 0; i < word.size(); ++i) {
        if (i)
            fputc(' ', out);
        fprintf(out, "%d", word[i].g);
        for (int j = 2; j < word[i].m; ++j)
            fputc('*', out);
    }
}
static void emit(const State& s, int lost, int nontri)
{
    int F = (N - 1) * (N - 2) / 2 - lost;
    int cnt[MAXN + 1] = { 0 };

    for (auto& l : word)
        ++cnt[l.m];
    for (size_t i = 0; i < word.size(); ++i) // full word: all simple legs are decided
        if (word[i].m == 3 && !LEGS_INDEPENDENT[s.vertex[i].simple_legs.v])
            ++legs_bad;
    printf("[%.1fs] K=%d", elapsed(), F - nontri);
    for (int m = 3; m <= N; ++m) {
        if (m > 3 && !cnt[m])
            continue;
        putchar(' ');
        for (int k = 0; k < m - 2; ++k)
            putchar('*');
        printf("int=%d", cnt[m]);
    }
    printf(" i=%llu", (unsigned long long)visited_nodes);
    if (PRINT_LEDGER) { // u, 2*d3, d4 for comparison with an independent counter
        int U = s.unused;
        int d3x2 = 0;
        int d4 = 0;
        for (FaceIndex c = 0; c <= N; ++c)
            U += s.pending[c];
        for (size_t i = 0; i < word.size(); ++i)
            if (word[i].m == 3)
                d3x2 += 2 * s.vertex[i].lambda2 + s.vertex[i].lambda3;
            else if (word[i].m >= 4)
                d4 += s.vertex[i].lambda2;
        printf(" u=%d d3x2=%d d4=%d", U, d3x2, d4);
    }
    printf(") ");
    fput_word(stdout);
    putchar('\n');
    fflush(stdout);
}

// ------------------------------------------------------------- search ---
// Search(W) of Section 3: traverses the subtree of the partial word W. Notation:
// done_pairs = P(W),
// spent_faces = lost_faces + nontri_faces = S(W)
//     (multiple points and non-triangular faces),
// BUDGET_FACES = B,
// TOTAL_PAIRS = C(n,2),
// TARGET_FACES = K,
// q = deltaS(W), with --pruning delta+S(W) (Appendix C),
// s.a[i] = a[i];
// g_prev: the first slot of the last letter of W (-1 for the empty word).
static void search(const State& s, int done_pairs, int lost_faces, int nontri_faces, bool has_required, Slot g_prev)
{
    if (interrupted)
        throw Stop { "signal" };
    if (MAX_NODES && visited_nodes >= MAX_NODES)
        throw Stop { "nodes" };

    ++visited_nodes;

    if (!QUIET && (visited_nodes & ((1u << 28) - 1)) == 0) {
        fprintf(stderr, "  ... nodes=%llu t=%.0fs found=%llu chunk: ",
            (unsigned long long)visited_nodes, elapsed(), (unsigned long long)found);
        fput_word(stderr);
        fputc('\n', stderr);
        fflush(stderr);
    }

    int spent_faces = lost_faces + nontri_faces;
    if (spent_faces > BUDGET_FACES)
        return;

    if (done_pairs == TOTAL_PAIRS) {
        if (REQUIRE_MULT && !has_required)
            return;
        ++found;
        emit(s, lost_faces, nontri_faces);
        if (FIRST_HIT)
            throw Stop { "witness" };
        return;
    }

    // Early count of non-triangles: open faces that will close as non-triangles.
    // The required multiplicity costs tri_loss(REQUIRE_MULT).
    int q = DOOM_ON ? (s.will_close & s.not_triangle).count() : 0;
    int required = REQUIRE_MULT && !has_required ? tri_loss(REQUIRE_MULT) : 0;
    if (spent_faces + q + required > BUDGET_FACES) {
        ++doom_cuts;
        return;
    }
    if (SEGMENTS_CUTOFF) {
        // f bounds the future triple points (Lemma C.9); the required multiplicity
        // is subtracted from the budget only for REQUIRE_MULT >= 4.
        int budget_left = BUDGET_FACES - spent_faces - q - (REQUIRE_MULT >= 4 ? required : 0);
        int triple_ceiling = std::min((TOTAL_PAIRS - done_pairs) / 3, budget_left < 0 ? 0 : budget_left);
        int u = s.unused;
        // Early count: the pending segments at doomed faces. Same as
        //     if (dm.test(c)) u += s.pending[c];
        // but without a branch: -int(bool) is a mask of all ones or zero. Doom
        // changes almost randomly from node to node, and the branch is poorly
        // predicted.
        const FaceBits dm = doomed_mask(s);
        for (FaceIndex c = 0; c <= N; ++c)
            u += s.pending[c] & -int(dm.test(c));
        // (C.6): 2u <= 2b - sum 2 ch*_lb(v) + 3f
        if (2 * u > 2 * WASTE_LIMIT - charges_sum(s) + 3 * triple_ceiling) {
            ++segment_cuts;
            return;
        }
    }
    if (REQUIRE_MULT && !has_required && TOTAL_PAIRS - done_pairs < pairs(REQUIRE_MULT))
        return;
    if (PREFIX_DEPTH >= 0 && int(word.size()) >= PREFIX_DEPTH) {
        ++prefixes;
        fput_word(stdout);
        putchar('\n');
        return;
    }
    // The key and the statuses of the node serve both the table probe and the store on return.
    uint64_t node_key = 0;
    uint32_t node_statuses = 0;
    if (TABLE_MODE) {
        node_statuses = pack_statuses(s);
        node_key = table_key(pack_wires(s), g_prev, has_required);
        if (dominated(s, spent_faces, node_key, node_statuses))
            return;
    }

    const uint64_t found_before = found;
    bool forced = is_forced_descend(s, g_prev);
    for (Mult m = 2; m <= MAX_MULT; ++m) {
        if (REDUCED_ON && m >= 5)
            break;
        if (spent_faces + tri_loss(m) > BUDGET_FACES || pairs(m) > TOTAL_PAIRS - done_pairs)
            break;

        Slot lo;
        Slot hi;

        if (forced) {
            lo = hi = g_prev - m + 1;
            if (lo < 0)
                continue;
        }
        else {
            lo = std::max(0, g_prev - m + 1);
            hi = N - m;
            if (g_prev < 0 && FIRST_FIXED)
                hi = 0;

            if (lo > hi)
                continue;
        }

        FaceBits starts = s.asc; // the block g..g+m-1 ascends
        for (int k = 1; k < m - 1; ++k)
            starts &= s.asc >> k;
        starts.and_mask(FaceBits::span(lo, hi)); // starts no earlier than lo and no later than hi

        for (Slot g : starts) {
            State child = s;
            int new_nontri = apply(child, g, m, int(word.size()));
            if (spent_faces + tri_loss(m) + new_nontri > BUDGET_FACES)
                continue;
            if (child.reduced_viol) {
                ++reduced_cuts;
                continue;
            }
            push_letter(g, m);
            search(child, done_pairs + pairs(m), lost_faces + tri_loss(m), nontri_faces + new_nontri, has_required || (REQUIRE_MULT && m >= REQUIRE_MULT), g);
            pop_letter();
        }
    }
    // The subtree is fully traversed (Stop does not get here). Without words the
    // entry is stored CLEAN. Entries are inserted only here and never edited, so
    // the shared table needs no CAS: a race costs a cut, not a word.
    if (TABLE_MODE && found == found_before) {
        Entry* b = bucket(node_key);
        Entry cell[TABLE_WAYS];
        for (uint64_t w = 0; w < TABLE_WAYS; ++w)
            cell[w] = load_entry(&b[w]);
        uint64_t victim = 0;
        bool empty = false;
        for (uint64_t w = 0; w < TABLE_WAYS; ++w)
            if (!cell[w].key) {
                victim = w;
                empty = true;
                break;
            }
        if (!empty) {
            long long same = -1;
            for (uint64_t w = 0; w < TABLE_WAYS; ++w) {
                if (cell[w].key == node_key && entry_spent(cell[w]) >= spent_faces && (same < 0 || entry_spent(cell[w]) > entry_spent(cell[same])))
                    same = (long long)w;
                if (entry_spent(cell[w]) > entry_spent(cell[victim]))
                    victim = w;
            }
            if (same >= 0)
                victim = (uint64_t)same;
        }
        if (cell[victim].key && cell[victim].key != node_key)
            ++evictions;
        store_entry(&b[victim], node_key, node_statuses, spent_faces | (REDUCED_ON ? int(word.size()) << V_SHIFT : 0) | CLEAN);
    }
}

// ---------------------------------------------------------------- main ---
static uint64_t integer(const std::string& s, uint64_t max)
{
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("expected a non-negative integer: " + s);
    size_t pos = 0;
    auto v = std::stoull(s, &pos);
    if (pos != s.size() || v > max)
        throw std::runtime_error("number out of range: " + s);
    return v;
}
// Table memory: 1G / 256M / 64K or a number of bytes. Rounded down to a power
// of two entries of sizeof(Entry) = 16 bytes.
static uint64_t entries(const std::string& v)
{
    uint64_t mul = 1;
    std::string d = v;
    if (!d.empty() && (d.back() == 'K' || d.back() == 'M' || d.back() == 'G')) {
        mul = d.back() == 'K' ? 1ull << 10 : d.back() == 'M' ? 1ull << 20
                                                             : 1ull << 30;
        d.pop_back();
    }
    uint64_t e = integer(d, 1ull << 40) * mul / sizeof(Entry);
    if (e < TABLE_WAYS)
        throw std::runtime_error("table too small: " + v);
    // The number of buckets is a power of two: bucket() splits the key by a mask.
    // The cap of 2^31 entries is 32 GiB; larger was not tried.
    uint64_t p = TABLE_WAYS;
    while (p * 2 <= e && p < (1ull << 31))
        p *= 2;
    return p;
}
static const char* USAGE = "usage: multenum <n> <K> [options]\n"
                           "  zone:     --max-mult M  --require-mult M\n"
                           "  mode:     --first  --quiet  --max-nodes N  --stop-on-word\n"
                           "  scope:    --first-fixed   first letter (0,m) only (Sec. 3, first letter)\n"
                           "  reduce:   --reduced   Roudneff normalization: verdict/first witness only\n"
                           "  speed-up: --pruning [memory]   everything beyond Sec. 3 (App. C): 1G, 256M\n"
                           "            --pruning-shm NAME   share that table via /dev/shm/NAME,\n"
                           "                                 or /path/on/hugetlbfs/NAME for huge pages\n"
                           "  chunking: --prefixes D  --chunk \"word\"  --chunks FILE  --log FILE\n"
                           "  checks:   --no-forced-descent --no-doom\n"
                           "            --no-segments --no-table --no-pair-doom --table-equal --ledger\n"
                           "  profile:  --reuse-stats   (table reuse distances; needs --pruning)\n";
int main(int argc, char** argv)
{
    try {
        if (argc < 3) {
            fputs(USAGE, stderr);
            return 1;
        }
        N = int(integer(argv[1], MAXN));
        if (N < 3)
            throw std::runtime_error("3 <= n <= " + std::to_string(MAXN) + " required");
        TARGET_FACES = int(integer(argv[2], 1000));
        MAX_MULT = N;
        KEY_G_PREV_SHIFT = std::max(48, 4 * N);
        KEY_BIG_SHIFT = KEY_G_PREV_SHIFT + 4;
        std::string chunk;
        bool chunk_given = false;
        for (int i = 3; i < argc; ++i) {
            std::string k = argv[i];
            if (k == "--first") {
                FIRST_HIT = true;
                continue;
            }
            if (k == "--stop-on-word") {
                STOP_ON_WORD = true;
                continue;
            }
            if (k == "--quiet") {
                QUIET = true;
                continue;
            }
            if (k == "--ledger") {
                PRINT_LEDGER = true;
                continue;
            }
            if (k == "--reduced") {
                REDUCED_ON = true;
                continue;
            }
            if (k == "--reuse-stats") {
                REUSE_STATS = true;
                continue;
            }
            if (k == "--pruning") {
                SEGMENTS_CUTOFF = true;
                TABLE_MODE = 2;
                PAIR_DOOM_ON = true;
                PRUNING_ARG = "256M";
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    PRUNING_ARG = argv[++i];
                    TABLE_SIZE = entries(PRUNING_ARG);
                }
                continue;
            }
            if (k == "--no-segments") {
                NO_SEGMENTS = true;
                continue;
            }
            if (k == "--no-pair-doom") {
                NO_PAIR_DOOM = true;
                continue;
            }
            if (k == "--no-table") {
                NO_TABLE = true;
                continue;
            }
            if (k == "--table-equal") {
                TABLE_EQUAL = true;
                continue;
            }
            if (k == "--first-fixed") {
                FIRST_FIXED = true;
                continue;
            }
            if (k == "--no-forced-descent") {
                FORCED_DESCENT_ON = false;
                continue;
            }
            if (k == "--no-doom") {
                DOOM_ON = false;
                continue;
            }
            if (k != "--max-mult" && k != "--require-mult" && k != "--prefixes" && k != "--chunk" && k != "--chunks" && k != "--max-nodes" && k != "--pruning-shm" && k != "--log")
                throw std::runtime_error("unknown option: " + k);
            if (i + 1 >= argc)
                throw std::runtime_error("missing value for " + k);
            std::string v = argv[++i];
            if (k == "--max-mult")
                MAX_MULT = int(integer(v, N));
            else if (k == "--require-mult")
                REQUIRE_MULT = int(integer(v, N));
            else if (k == "--prefixes")
                PREFIX_DEPTH = int(integer(v, MAXW));
            else if (k == "--chunk") {
                chunk = v;
                chunk_given = true;
            } else if (k == "--chunks")
                CHUNKS_FILE = v;
            else if (k == "--pruning-shm") {
                if (v.empty())
                    throw std::runtime_error("--pruning-shm needs a segment name");
                PRUNING_SHM = v;
            } else if (k == "--log")
                LOG_FILE = v;
            else if (k == "--max-nodes")
                MAX_NODES = integer(v, 1ull << 62);
            else
                throw std::runtime_error("unknown option: " + k);
        }
        // The ablations disable layers of --pruning and serve only the checks.
        if ((NO_SEGMENTS || NO_TABLE || TABLE_EQUAL || NO_PAIR_DOOM || PRINT_LEDGER) && !PAIR_DOOM_ON)
            throw std::runtime_error("ablations and --ledger require --pruning");
        if (PRINT_LEDGER && NO_SEGMENTS)
            throw std::runtime_error("--ledger prints the segment count and conflicts with --no-segments");
        if (NO_PAIR_DOOM)
            PAIR_DOOM_ON = false;
        if (NO_SEGMENTS)
            SEGMENTS_CUTOFF = false;
        if (NO_TABLE)
            TABLE_MODE = 0;
        if (TABLE_EQUAL)
            TABLE_MODE = 1;
        if (MAX_MULT < 2 || (REQUIRE_MULT && (REQUIRE_MULT < 3 || REQUIRE_MULT > MAX_MULT)))
            throw std::runtime_error("invalid multiplicity bounds");
        if (REDUCED_ON && REQUIRE_MULT)
            throw std::runtime_error("--reduced perturbs multiple points and conflicts with --require-mult");
        // Guard against gigabytes of output: the number of words grows fast with b;
        // working runs have b <= 6.
        if (N >= 8 && N * (N - 2) - 3 * TARGET_FACES > 12)
            throw std::runtime_error("for n >= 8 the target must satisfy n(n-2) - 3K <= 12");
        if (!CHUNKS_FILE.empty() && (!chunk.empty() || PREFIX_DEPTH >= 0))
            throw std::runtime_error("--chunks conflicts with --chunk and --prefixes");
        // An empty chunk would mean the whole tree.
        if (chunk_given && chunk.find_first_not_of(' ') == std::string::npos)
            throw std::runtime_error("--chunk is empty");
        if (!PRUNING_SHM.empty() && !TABLE_MODE)
            throw std::runtime_error("--pruning-shm requires the state table (--pruning)");
        if (!PRUNING_SHM.empty() && PREFIX_DEPTH >= 0)
            throw std::runtime_error("--pruning-shm conflicts with --prefixes");
        // A path is allowed: that puts the segment on hugetlbfs (see table_alloc).
        if (PREFIX_DEPTH >= 0)
            TABLE_MODE = 0; // chunking runs over the full tree
        init_leg_tables();
        TOTAL_PAIRS = pairs(N);
        BUDGET_FACES = (N - 1) * (N - 2) / 2 - TARGET_FACES;
        WASTE_LIMIT = N * (N - 2) - 3 * TARGET_FACES;
        if (TABLE_MODE)
            table_alloc(); // after budget: it is part of the segment fingerprint

        State root;
        // Minimum and maximum of the empty set: left_min[0] = n (+inf, never changes),
        // right_max[n] = 0 (-inf, zero from the initialization).
        root.left_min[0] = uint8_t(N);
        for (FaceIndex c = 0; c < N; ++c)
            root.right_max[c] = uint8_t(N - 1);
        for (int i = 0; i < N; ++i)
            root.a[i] = uint8_t(i);
        // last[] is already initialized to NO_LAST by the WireCross initializers.
        for (FaceIndex c = 1; c < N; ++c)
            root.will_close.set(c);
        for (int i = 0; i + 1 < N; ++i)
            root.asc.set(i);
        // Replay the prefix checking that it is allowed, then traverse its subtree.
        // With --chunks once per line; the table is not reset between chunks.
        auto run_chunk = [&](const std::string& c) {
            State s = root;
            word.clear();
            multi.clear();
            int done = 0;
            int lost = 0;
            int nontri = 0;
            Slot g_prev = -1;
            // The word already has a letter of multiplicity >= REQUIRE_MULT (only with --require-mult).
            bool has_required = false;
            for (size_t pos = 0; pos < c.size();) {
                while (pos < c.size() && c[pos] == ' ')
                    ++pos;
                if (pos == c.size())
                    break;
                size_t end = c.find(' ', pos);
                if (end == std::string::npos)
                    end = c.size();
                auto tok = c.substr(pos, end - pos);
                pos = end;
                int m = 2;
                while (!tok.empty() && tok.back() == '*') {
                    tok.pop_back();
                    ++m;
                }
                int g = int(integer(tok, N));
                if (!legal(s, g_prev, g, m, is_forced_descend(s, g_prev)))
                    throw std::runtime_error("illegal letter in chunk: " + c);
                if (REDUCED_ON && m >= 5)
                    return; // the prefix is not reduced
                nontri += apply(s, g, m, int(word.size()));
                done += pairs(m);
                lost += tri_loss(m);
                has_required = has_required || (REQUIRE_MULT && m >= REQUIRE_MULT);
                g_prev = g;
                push_letter(g, m);
            }
            if (s.reduced_viol)
                return; // the prefix itself is not reduced
            search(s, done, lost, nontri, has_required, g_prev);
        };
        fprintf(stderr, "multenum n=%d K>=%d zone=%s flags=%s%s%s", N, TARGET_FACES,
            zone_str(), flags_str(), chunk.empty() ? "" : " chunk=", chunk.c_str());
        if (TABLE_MODE)
            fprintf(stderr, " table=%llu entries", (unsigned long long)TABLE_SIZE);
        if (TABLE_MODE && !PRUNING_SHM.empty())
            fprintf(stderr, " shm=%s", PRUNING_SHM.c_str());
        fputc('\n', stderr);
        std::signal(SIGINT, [](int) { interrupted = 1; });
        std::signal(SIGTERM, [](int) { interrupted = 1; });
        start = std::chrono::steady_clock::now();
        // PREFIXES is not a verdict: only the chunking is done. With --chunk the
        // verdict refers to one subtree (field chunk=).
        const char* status = PREFIX_DEPTH >= 0 ? "PREFIXES" : "DONE";
        const char* reason = "";
        try {
            if (CHUNKS_FILE.empty())
                run_chunk(chunk);
            else {
                std::ifstream f(CHUNKS_FILE);
                if (!f)
                    throw std::runtime_error("cannot open " + CHUNKS_FILE);
                std::string line;
                while (std::getline(f, line)) {
                    while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                        line.pop_back();
                    if (line.empty())
                        continue;
                    uint64_t nodes0 = visited_nodes;
                    uint64_t found0 = found;
                    run_chunk(line);
                    ++chunks_done;
                    log_chunk(line, 0, visited_nodes - nodes0, found - found0);
                }
            }
        } catch (const Stop& x) {
            reason = x.reason;
            status = strcmp(reason, "witness") ? "INCOMPLETE" : "DONE";
        }
        fprintf(stderr, "%s n=%d K>=%d zone=%s flags=%s%s%s: nodes=%llu words=%llu time=%.0fs",
            status, N, TARGET_FACES, zone_str(), flags_str(), chunk.empty() ? "" : " chunk=", chunk.c_str(),
            (unsigned long long)visited_nodes, (unsigned long long)found, elapsed());
        if (*reason)
            fprintf(stderr, " stop=%s", reason);
        if (PREFIX_DEPTH >= 0)
            fprintf(stderr, " prefixes=%llu", (unsigned long long)prefixes);
        if (!CHUNKS_FILE.empty())
            fprintf(stderr, " chunks=%llu", (unsigned long long)chunks_done);
        if (DOOM_ON)
            fprintf(stderr, " doom-cuts=%llu", (unsigned long long)doom_cuts);
        if (REDUCED_ON)
            fprintf(stderr, " reduced-cuts=%llu", (unsigned long long)reduced_cuts);
        if (SEGMENTS_CUTOFF)
            fprintf(stderr, " segment-cuts=%llu", (unsigned long long)segment_cuts);
        if (TABLE_MODE)
            fprintf(stderr, " table-probes=%llu table-hits=%llu table-cuts=%llu",
                (unsigned long long)table_probes, (unsigned long long)table_hits, (unsigned long long)table_cuts);
        if (REUSE_STATS)
            fputs(reuse_report().c_str(), stderr);
        if (SEGMENTS_CUTOFF)
            fprintf(stderr, " lemma-violations=%llu/%llu/%llu",
                (unsigned long long)endpoint_bad, (unsigned long long)legs_bad, (unsigned long long)early_bad);
        fputc('\n', stderr);
        // 255 stops xargs: a word was found (--stop-on-word) or Lemmas C.2, C.4, C.8
        // are violated, and then the run is invalid.
        int code = strcmp(status, "INCOMPLETE") ? 0 : 2;
        if (STOP_ON_WORD && found)
            code = 255;
        if (endpoint_bad || legs_bad || early_bad) {
            fprintf(stderr, "ERROR segment-count lemmas violated: %llu/%llu/%llu\n",
                (unsigned long long)endpoint_bad, (unsigned long long)legs_bad,
                (unsigned long long)early_bad);
            code = 255;
        }
        if (CHUNKS_FILE.empty() && PREFIX_DEPTH < 0)
            log_chunk(chunk, code, visited_nodes, found);
        return code;
    } catch (const std::exception& e) {
        fprintf(stderr, "ERROR %s\n", e.what());
        return 1;
    }
}
