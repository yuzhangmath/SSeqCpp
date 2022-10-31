/** \file groebnerZ.h
 * A Component for Groebner bases.
 */

#ifndef GROEBNERZ_H
#define GROEBNERZ_H

#include "algebrasZ.h"
#include "groebner.h"
#include <map>
#include <unordered_set>

/* extension of namespace alg in algebras.h */
namespace algZ {

namespace detail {
    /*
     * Return if `mon1` and `mon2` have a nontrivial common factor.
     */
    inline bool HasGCD(const Mon& mon1, const Mon& mon2)
    {
        return std::min(mon1.c(), mon2.c()) > 0 || alg2::detail::HasGCD(mon1.m0(), mon2.m0()) || alg2::detail::HasGCD(mon1.m1(), mon2.m1());
    }
    inline bool HasGCD(const Mon& mon1, const Mon& mon2, MonTrace t1, MonTrace t2)
    {
        return (t1 & t2) && HasGCD(mon1, mon2);
    }

    inline int DegLCM(const Mon& mon1, const Mon& mon2, const int1d& gen_degs)
    {
        return alg2::detail::DegLCM(mon1.m0(), mon2.m0(), gen_degs) + alg2::detail::DegLCM(mon1.m1(), mon2.m1(), gen_degs) + gen_degs[0] * std::max(mon1.c(), mon2.c());
    }
}  // namespace detail

class Groebner;
class GroebnerMod;

inline constexpr uint32_t NULL_INDEX32 = ~uint32_t(0);
inline constexpr uint32_t FLAG_INDEX_X = uint32_t(1) << 31;

struct CriPair
{
    uint32_t i1 = NULL_INDEX32, i2 = NULL_INDEX32;
    Mon m1, m2;

    MonTrace trace_m2 = 0; /* = Trace(m2) */

    /* Compute the pair for two leading monomials. */
    CriPair() = default;
    static void SetFromLM(CriPair& result, const Mon& lead1, const Mon& lead2, int i, int j, const int1d& gen_fils);

    /* Return `m1 * gb[i1] + m2 * gb[i2]` */
    void SijP(const Groebner& gb, Poly& result, Poly& tmp) const;
    void SijMod(const Groebner& gb, const GroebnerMod& gbm, Mod& result, Mod& tmp) const;
};
using CriPair1d = std::vector<CriPair>;
using CriPair2d = std::vector<CriPair1d>;

/* Groebner basis of critical pairs */
class GbCriPairs
{
private:
    int deg_trunc_;                                                      /* Truncation degree */
    CriPair2d gb_;                                                       /* `pairs_[j]` is the set of pairs (i, j) with given j */
    std::map<int, CriPair2d> buffer_min_pairs_;                          /* To generate `buffer_min_pairs_` and for computing Sij */
    std::map<int, std::unordered_set<uint64_t>> buffer_redundent_pairs_; /* Used to minimize `buffer_min_pairs_` */

public:
    GbCriPairs(int d_trunc) : deg_trunc_(d_trunc) {}
    int deg_trunc() const
    {
        return deg_trunc_;
    }
    CriPair1d Criticals(int d)
    {
        CriPair1d result;
        if (!buffer_min_pairs_.empty() && buffer_min_pairs_.begin()->first == d) {
            auto& b_min_pairs_d = buffer_min_pairs_.begin()->second;
            for (size_t j = 0; j < b_min_pairs_d.size(); ++j)
                for (auto& pair : b_min_pairs_d[j])
                    if (pair.i2 != NULL_INDEX32)
                        result.push_back(std::move(pair));
            buffer_min_pairs_.erase(buffer_min_pairs_.begin());
        }
        else if (!buffer_min_pairs_.empty() && buffer_min_pairs_.begin()->first < d)
            throw MyException(0x3321786aU, "buffer_min_pairs_ contains degree < d");
        return result;
    }
    int NextD() const
    {
        return buffer_min_pairs_.empty() ? -1 : buffer_min_pairs_.begin()->first;
    }
    bool empty() const
    {
        return buffer_min_pairs_.empty();
    }

    void Reset()
    {
        gb_.clear();
        buffer_min_pairs_.clear();
        buffer_redundent_pairs_.clear();
    }

    /* Minimize `buffer_min_pairs_[d]` and maintain `pairs_` */
    void Minimize(const Mon1d& leads, int d);
    void Minimize(const Mon1d& leadsx, const MMod1d& leads, int d);

    /* Propogate `buffer_redundent_pairs_` and `buffer_min_pairs_`.
    ** `buffer_min_pairs_` will become a Groebner basis at this stage.
    */
    void AddToBuffers(const Mon1d& leads, const MonTrace1d& traces, const Mon& mon, const AdamsDeg1d& gen_degs);
    void AddToBuffers(const Mon1d& leadsx, const MonTrace1d& tracesx, const MMod1d& leads, const MonTrace1d& traces, const MMod& mon, const AdamsDeg1d& gen_degs, const AdamsDeg1d& v_degs);
    void AddToBuffersX(const Mon1d& leadsx, const MonTrace1d& tracesx, const MMod1d& leads, const MonTrace1d& traces, const AdamsDeg1d& gen_degs, const AdamsDeg1d& v_degs, size_t i_start);
    void init(const Mon1d& leads, const MonTrace1d& traces, const AdamsDeg1d& gen_degs, int t_min_buffer);
    void init(const Mon1d& leadsx, const MonTrace1d& tracesx, const MMod1d& leads, const MonTrace1d& traces, const AdamsDeg1d& gen_degs, const AdamsDeg1d& v_degs, int t_min_buffer);
};

/* 2 is considered as the first generator */
class Groebner
{
private:
    using TypeIndexKey = uint32_t;
    friend class GroebnerMod;

private:
    GbCriPairs criticals_; /* Groebner basis of critical pairs */

    Poly1d data_;
    Mon1d leads_;                                                /* Leading monomials */
    MonTrace1d traces_;                                          /* Cache for fast divisibility test */
    std::unordered_map<TypeIndexKey, int1d> leads_group_by_key_; /* Cache for fast divisibility test */
    int2d leads_group_by_last_gen_;                              /* Cache for generating a basis */
    std::map<AdamsDeg, int1d> leads_group_by_deg_;               /* Cache for generating a basis */

    AdamsDeg1d gen_degs_; /* degree of generators */
    int1d gen_2tor_degs_; /* 2 torsion degree of generators */

public:
    Groebner() : criticals_(DEG_MAX), gen_degs_({AdamsDeg(1, 1)}), gen_2tor_degs_({FIL_MAX}) {}
    Groebner(int deg_trunc, AdamsDeg1d gen_degs) : criticals_(deg_trunc), gen_degs_(std::move(gen_degs)), gen_2tor_degs_(gen_degs_.size(), FIL_MAX) {}

    /* Initialize from `polys` which already forms a Groebner basis. The instance will be in const mode. */
    Groebner(int deg_trunc, AdamsDeg1d gen_degs, Poly1d polys, bool bDynamic = false);

private:
    static TypeIndexKey Key(const Mon& lead)
    {
        return TypeIndexKey{(lead.m0() ? lead.m0().backg() + 1 : 0) + (lead.m1() ? ((lead.m1().backg() + 1) << 16) : 0)};
    }

public: /* Getters and Setters */
    const GbCriPairs& gb_pairs() const
    {
        return criticals_;
    }
    int deg_trunc() const
    {
        return criticals_.deg_trunc();
    }
    /* This function will erase `gb_pairs_.buffer_min_pairs[d]` */
    CriPair1d Criticals(int d)
    {
        criticals_.Minimize(leads_, d);
        return criticals_.Criticals(d);
    }

    auto size() const
    {
        return data_.size();
    }
    const auto& operator[](size_t index) const
    {
        return data_[index];
    }
    int push_back_data_init(Poly g)
    {
        const Mon& m = g.GetLead();
        AdamsDeg deg = GetDeg(m, gen_degs_);
        if (deg.t <= criticals_.deg_trunc()) {
            leads_.push_back(m);
            traces_.push_back(m.Trace());
            int index = (int)data_.size();
            leads_group_by_key_[Key(m)].push_back(index);
            leads_group_by_deg_[deg].push_back(index);
            ut::push_back(leads_group_by_last_gen_, (size_t)m.backg(), index);

            if (g.data.size() == 1) {
                if (m.frontg() == m.backg() && ((m.m0() && m.m0().begin()->e() == 1) || m.m1().begin()->e() == 1)) {
                    size_t index = (size_t)m.frontg();
                    gen_2tor_degs_[index] = std::min(m.c(), gen_2tor_degs_[index]);
                }
            }

            data_.push_back(std::move(g));
            return 1;
        }
        else
            return 0;
    }
    void set_gen_2tor_degs_for_S0()
    {
        for (size_t i = 1; i < gen_degs_.size(); ++i) {
            gen_2tor_degs_[i] = std::min(gen_2tor_degs_[i], (gen_degs_[i].stem() + 3) / 2);
        }
    }
    void push_back_data(Poly g)
    {
        Mon m = g.GetLead();
        if (push_back_data_init(std::move(g)))
            criticals_.AddToBuffers(leads_, traces_, m, gen_degs_);
    }
    bool operator==(const Groebner& rhs) const
    {
        return data_ == rhs.data_;
    }

    const auto& data() const
    {
        return data_;
    }

    const auto& leads() const
    {
        return leads_;
    }

    const auto& gen_degs() const
    {
        return gen_degs_;
    }

    const auto& gen_2tor_degs() const
    {
        return gen_2tor_degs_;
    }

    const auto& leads_group_by_deg() const
    {
        return leads_group_by_deg_;
    }

    const std::map<AdamsDeg, Poly1d> OutputForDatabase() const
    {
        std::map<AdamsDeg, Poly1d> result;
        for (auto& [deg, indices] : leads_group_by_deg_) {
            for (int i : indices)
                result[deg].push_back(data_[i]);
        }
        return result;
    }

    /* Return trace of LM(gb[i]) */
    MonTrace GetTrace(size_t i) const
    {
        return traces_[i];
    }

public:
    /* Compute the basis in `deg` assuming all basis have been calculated in lower total degrees
     * The relations in `deg` are ignored
     */
    Mon1d GenBasis(AdamsDeg deg, const std::map<AdamsDeg, Mon1d>& basis) const;

    /* Return relations with leadings with the given deg */
    Poly1d RelsLF(AdamsDeg deg) const;

    /* Return -1 if not found */
    int IndexOfDivisibleLeading(const Mon& mon, int eff_min) const;

    /* This function does not decrease the certainty of poly */
    Poly Reduce(Poly poly) const;
    /* This reduce poly by gb with all certainties until the lead is in the basis */
    Poly ReduceV2(Poly poly) const;

    void AddGen(AdamsDeg deg)
    {
        gen_degs_.push_back(deg);
    }

    /**
     * Comsume relations from 'rels` and `gb.criticals_` in degree `<= deg`
     */
    void AddRels(const Poly1d& rels, int deg);

    void ResetRels()
    {
        criticals_.Reset();
        leads_.clear();
        traces_.clear();
        leads_group_by_key_.clear();
        leads_group_by_deg_.clear();
        leads_group_by_last_gen_.clear();
        data_.clear();
    }

    void SimplifyRels()
    {
        Poly1d data1 = std::move(data_);
        ResetRels();
        AddRels(data1, criticals_.deg_trunc());
    }
};

/**
 * A fast algorithm that computes
 * `poly ** n` modulo `gb`
 */
void powP(const Poly& poly, int n, const Groebner& gb, Poly& result, Poly& tmp);
inline Poly pow(const Poly& poly, int n, const Groebner& gb)
{
    Poly result, tmp;
    powP(poly, n, gb, result, tmp);
    return result;
}

/**
 * Replace the generators in `poly` with elements given in `map`
 * and evaluate modulo `gb`.
 * @param poly The polynomial to be substituted.
 * @param map `map(i)` is the polynomial that substitutes the generator of id `i`.
 * @param gb A Groebner basis.
 */
template <typename FnMap>
Poly subsMGbTpl(const Poly& poly, const Groebner& gb, const FnMap& map)
{
    Poly result, tmp_prod, tmp;
    for (const Mon& m : poly.data) {
        Poly fm = Poly::twoTo(m.c());
        for (auto p = m.m0().begin(); p != m.m0().end(); ++p) {
            powP(map(p->g()), p->e(), gb, tmp_prod, tmp);
            fm.imulP(tmp_prod, tmp);
        }
        for (auto p = m.m1().begin(); p != m.m1().end(); ++p) {
            powP(map(p->g()), p->e(), gb, tmp_prod, tmp);
            fm.imulP(tmp_prod, tmp);
        }
        fm = gb.Reduce(std::move(fm));
        result.iaddP(fm, tmp);
    }
    return result;
}

inline Poly subsMGb(const Poly& poly, const Groebner& gb, const std::vector<Poly>& map)
{
    return subsMGbTpl(poly, gb, [&map](size_t i) { return map[i]; });
}

/********************************* Modules ****************************************/

class GroebnerMod
{
private:
    using TypeIndexKey = uint32_t;

private:
    Groebner* pGb_;
    size_t old_pGb_size;
    GbCriPairs criticals_; /* Groebner basis of critical pairs */

    Mod1d data_;
    MMod1d leads_;                                               /* Leading monomials */
    MonTrace1d traces_;                                          /* Cache for fast divisibility test */
    std::unordered_map<TypeIndexKey, int1d> leads_group_by_key_; /* Cache for fast divisibility test */
    int2d leads_group_by_v_;                                     /* Cache for generating a basis */
    std::map<AdamsDeg, int1d> leads_group_by_deg_;               /* Cache for generating a basis */

    AdamsDeg1d v_degs_; /* degree of generators of modules */

public:
    GroebnerMod() : pGb_(nullptr), criticals_(DEG_MAX), old_pGb_size(0) {}
    GroebnerMod(Groebner* pGb, int deg_trunc, AdamsDeg1d v_degs) : pGb_(pGb), criticals_(deg_trunc), v_degs_(std::move(v_degs)), old_pGb_size(pGb->leads_.size()) {}

    /* Initialize from `polys` which already forms a Groebner basis. The instance will be in const mode. */
    GroebnerMod(Groebner* pGb, int deg_trunc, AdamsDeg1d v_degs, Mod1d polys, bool bDynamic = false);

private:
    static TypeIndexKey Key(const MMod& lead)
    {
        return TypeIndexKey{lead.v + (lead.m ? ((lead.m.backg() + 1) << 16) : 0)};
    }

public:
    /**
     * Transform to a submodule.
     *
     * `rels` should be ordered by degree.
     */
    void ToSubMod(const Mod1d& rels, int deg, int1d& index_ind);

public: /* Getters and Setters */
    const auto& gb_pairs() const
    {
        return criticals_;
    }
    int deg_trunc() const
    {
        return criticals_.deg_trunc();
    }
    CriPair1d Criticals(int d)
    {
        criticals_.Minimize(pGb_->leads_, leads_, d);
        return criticals_.Criticals(d);
    }

    auto size() const
    {
        return data_.size();
    }
    const auto& operator[](size_t index) const
    {
        return data_[index];
    }
    int push_back_data_init(Mod g)
    {
        const MMod& m = g.GetLead();
        AdamsDeg deg = GetDeg(m, pGb_->gen_degs(), v_degs_);
        if (deg.t <= criticals_.deg_trunc()) {
            leads_.push_back(m);
            traces_.push_back(m.m.Trace());
            int index = (int)data_.size();
            leads_group_by_key_[Key(m)].push_back(index);
            leads_group_by_deg_[deg].push_back(index);
            ut::push_back(leads_group_by_v_, (size_t)m.v, index);
            data_.push_back(std::move(g));
            return 1;
        }
        else
            return 0;
    }
    /* This is used for initialization */
    void push_back_data(Mod g)
    {
        MMod m = g.GetLead();
        if (push_back_data_init(std::move(g)))
            criticals_.AddToBuffers(pGb_->leads_, pGb_->traces_, leads_, traces_, m, pGb_->gen_degs(), v_degs_);
    }
    bool operator==(const GroebnerMod& rhs) const
    {
        return data_ == rhs.data_;
    }

    auto& data() const
    {
        return data_;
    }

    const auto& leads() const
    {
        return leads_;
    }

    auto& v_degs() const
    {
        return v_degs_;
    }

    const auto& leads_group_by_deg() const
    {
        return leads_group_by_deg_;
    }

    const std::map<AdamsDeg, Mod1d> OutputForDatabase() const
    {
        std::map<AdamsDeg, Mod1d> result;
        for (auto& [deg, indices] : leads_group_by_deg_) {
            for (int i : indices)
                result[deg].push_back(data_[i]);
        }
        return result;
    }

    /* Return trace of LM(gb[i]) */
    MonTrace GetTrace(size_t i) const
    {
        return traces_[i];
    }

public:
    /* Compute the basis in `deg` based on the basis of pGb_ */
    MMod1d GenBasis(AdamsDeg deg, const std::map<AdamsDeg, Mon1d>& basis) const;

    /* Return relations with leadings with the given deg */
    Mod1d RelsLF(AdamsDeg deg) const;

    /* Return -1 if not found */
    int IndexOfDivisibleLeading(const MMod& mon, int eff_min) const;

    /* This function does not decrease the certainty of poly */
    Mod Reduce(Mod poly) const;
    /* This reduce poly by gb with all certainties until the lead is in the basis */
    Mod ReduceV2(Mod poly) const;

    void AddGen(AdamsDeg deg)
    {
        v_degs_.push_back(deg);
    }

    /**
     * Comsume relations from 'rels` and `gb.criticals_` in degree `<= deg`
     */
    void AddRels(const Mod1d& rels, int deg);

    /* Update the Module when pGb_ changes */
    void UpdateFromGb()
    {
        criticals_.AddToBuffersX(pGb_->leads_, pGb_->traces_, leads_, traces_, pGb_->gen_degs(), v_degs_, old_pGb_size);
        AddRels({}, criticals_.deg_trunc());
        old_pGb_size = pGb_->leads_.size();
    }

    void ResetRels()
    {
        old_pGb_size = pGb_->leads_.size();
        criticals_.Reset();
        leads_.clear();
        traces_.clear();
        leads_group_by_key_.clear();
        leads_group_by_deg_.clear();
        leads_group_by_v_.clear();
        data_.clear();
    }

    void SimplifyRels()
    {
        Mod1d data1 = std::move(data_);
        ResetRels();
        AddRels(data1, criticals_.deg_trunc());
    }
};

}  // namespace algZ

#endif /* GROEBNER_H */
