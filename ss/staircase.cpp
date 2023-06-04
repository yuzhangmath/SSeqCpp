#include "algebras/linalg.h"
#include "main.h"
#include "mylog.h"

/* Add x, dx, level and triangularize.
 * Output the image of a differential that should be moved to the next level */
void triangularize(Staircase& sc, size_t i_insert, int1d x, int1d dx, int level, int1d& image, int& level_image)
{
    if (level == LEVEL_PERM + 1)
        __debugbreak();

    level_image = -1;

    size_t i = i_insert;
    while (!x.empty()) {
        std::swap(x, sc.basis[i]);
        std::swap(dx, sc.diffs[i]);
        std::swap(level, sc.levels[i]);

        ++i;
        for (size_t j = i_insert; j < i; ++j) {
            if (std::binary_search(x.begin(), x.end(), sc.basis[j][0])) {
                x = lina::add(x, sc.basis[j]);
                if (level == sc.levels[j] && dx != int1d{-1})
                    dx = lina::add(dx, sc.diffs[j]);
            }
        }
    }
    if (dx != int1d{-1} && !dx.empty()) {
        image = std::move(dx);
        level_image = LEVEL_MAX - level;
    }

    /* Triangularize the rest */
    for (; i < sc.basis.size(); ++i) {
        for (size_t j = i_insert; j < i; ++j) {
            if (std::binary_search(sc.basis[i].begin(), sc.basis[i].end(), sc.basis[j][0])) {
                sc.basis[i] = lina::add(sc.basis[i], sc.basis[j]);
                if (sc.levels[i] == sc.levels[j] && sc.diffs[i] != int1d{-1})
                    sc.diffs[i] = lina::add(sc.diffs[i], sc.diffs[j]);
            }
        }
#ifndef NDEBUG
        if (sc.basis[i].empty())
            throw MyException(0xfe35902dU, "BUG: triangularize()");
#endif
    }
}

size_t GetFirstIndexOnLevel(const Staircase& sc, int level)
{
    return std::lower_bound(sc.levels.begin(), sc.levels.end(), level) - sc.levels.begin();
}

size_t Diagram::GetFirstIndexOfNullOnLevel(const Staircase& sc, int level)
{
    int1d::const_iterator first = std::lower_bound(sc.levels.begin(), sc.levels.end(), level);
    int1d::const_iterator last = std::lower_bound(first, sc.levels.end(), level + 1);

    int1d::const_iterator it;
    ptrdiff_t count, step;
    count = std::distance(first, last);
    while (count > 0) {
        it = first;
        step = count / 2;
        std::advance(it, step);
        if (*(sc.diffs.begin() + (it - sc.levels.begin())) != int1d{-1}) {
            first = ++it;
            count -= step + 1;
        }
        else
            count = step;
    }
    return first - sc.levels.begin();
}

/* Return -1 if not found */
int Diagram::GetMaxLevelWithNull(const Staircase& sc)
{
    size_t i = sc.levels.size();
    while (i-- > 0) {
        if (sc.diffs[i] == int1d{-1})
            return sc.levels[i];
    }
    return -1;
}

/* Return if x is in the vector space <level */
bool Diagram::IsZeroOnLevel(const Staircase& sc, const int1d& x, int level)
{
    size_t first_l = GetFirstIndexOnLevel(sc, level);
    return lina::Residue(sc.basis.begin(), sc.basis.begin() + first_l, x).empty();
}

const Staircase& Diagram::GetRecentSc(const Staircases1d& nodes_ss, AdamsDeg deg)
{
    for (auto p = nodes_ss.rbegin(); p != nodes_ss.rend(); ++p)
        if (p->find(deg) != p->end())
            return p->at(deg);
    Logger::LogException(0, 0x553989e0U, "RecentStaircase not found. deg={}\n", deg);
    throw MyException(0x553989e0U, "RecentStaircase not found.");
}

/**
 * Apply the change of the staircase to the current history
 */
void Diagram::UpdateStaircase(Staircases1d& nodes_ss, AdamsDeg deg, const Staircase& sc_i, size_t i_insert, const int1d& x, const int1d& dx, int level, int1d& image, int& level_image)
{
    if (nodes_ss.back().find(deg) == nodes_ss.back().end())
        nodes_ss.back()[deg] = sc_i;
    triangularize(nodes_ss.back()[deg], i_insert, x, dx, level, image, level_image);
}