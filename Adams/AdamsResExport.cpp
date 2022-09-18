#include "algebras/benchmark.h"
#include "algebras/dbAdamsSS.h"
#include "algebras/groebner.h"
#include "algebras/linalg.h"
#include <cstring>
#include <map>

//#define MYDEPLOY

class MyDB : public myio::DbAdamsSS
{
    using Statement = myio::Statement;

public:
    MyDB() = default;
    explicit MyDB(const std::string& filename) : DbAdamsSS(filename) {}

public:
    void load_indecomposables(const std::string& table, alg::int1d& array_id, alg::AdamsDeg1d& gen_degs, int t_trunc) const
    {
        Statement stmt(*this, "SELECT id, s, t FROM " + table + " WHERE indecomposable=1 AND t<=" + std::to_string(t_trunc) + " ORDER BY id;");
        while (stmt.step() == MYSQLITE_ROW) {
            int id = stmt.column_int(0), s = stmt.column_int(1), t = stmt.column_int(2);
            array_id.push_back(id);
            gen_degs.push_back(alg::AdamsDeg{s, t});
        }
    }

    /**
     * loc2glo[s][i] = id
     * glo2loc[id] = (s, i)
     */
    void load_id_converter(const std::string& table, alg::int2d& loc2glo, alg::pairii1d& glo2loc, int t_trunc) const
    {
        Statement stmt(*this, "SELECT id, s FROM " + table + " WHERE t<=" + std::to_string(t_trunc) + " ORDER BY id;");
        while (stmt.step() == MYSQLITE_ROW) {
            int id = stmt.column_int(0), s = stmt.column_int(1);
            if (loc2glo.size() <= (size_t)s)
                loc2glo.resize(size_t(s + 1));
            loc2glo[s].push_back(id);
            glo2loc.push_back(std::make_pair(s, (int)(loc2glo[s].size() - 1)));
        }
    }

    /** id_ind * id = prod
     */
    std::map<std::pair<int, int>, alg::int1d> load_products_h(const std::string& table_prefix, int t_trunc) const
    {
        std::map<std::pair<int, int>, alg::int1d> result;
        Statement stmt(*this, "SELECT id, id_ind, prod_h FROM " + table_prefix + "_products LEFT JOIN " + table_prefix + "_generators USING(id) WHERE t<=" + std::to_string(t_trunc) + " ORDER BY id;");
        while (stmt.step() == MYSQLITE_ROW) {
            int id = stmt.column_int(0);
            int id_ind = stmt.column_int(1);
            alg::int1d prod_h = stmt.column_blob_tpl<int>(2);
            result[std::make_pair(id_ind, id)] = std::move(prod_h);
        }
        return result;
    }

    /** id_ind * id = prod
     */
    std::map<std::pair<int, int>, alg::int1d> load_cell_products_h(const std::string& table_prefix, int t_trunc) const
    {
        std::map<std::pair<int, int>, alg::int1d> result;
        Statement stmt(*this, "SELECT id, id_ind, prod_h FROM " + table_prefix + "_products LEFT JOIN " + table_prefix + "_E2 USING(id) WHERE t<=" + std::to_string(t_trunc) + " ORDER BY id;");
        while (stmt.step() == MYSQLITE_ROW) {
            int id = stmt.column_int(0);
            int id_ind = stmt.column_int(1);
            alg::int1d prod_h = myio::Deserialize<alg::int1d>(stmt.column_str(2));
            result[std::make_pair(id_ind, id)] = std::move(prod_h);
        }
        return result;
    }

    std::map<alg::AdamsDeg, alg::int2d> load_basis_repr(const std::string& table_prefix) const
    {
        std::map<alg::AdamsDeg, alg::int2d> result;
        Statement stmt(*this, "SELECT s, t, repr FROM " + table_prefix + "_basis ORDER BY id");
        int count = 0;
        while (stmt.step() == MYSQLITE_ROW) {
            ++count;
            alg::AdamsDeg deg = {stmt.column_int(0), stmt.column_int(1)};
            result[deg].push_back(myio::Deserialize<alg::int1d>(stmt.column_str(2)));
        }
        std::cout << "repr loaded from " << table_prefix << "_basis, size=" << count << '\n';
        return result;
    }
};

alg::int1d mul(const std::map<std::pair<int, int>, alg::int1d>& map_h, int id_ind, const alg::int1d& repr)
{
    alg::int1d result;
    for (int id : repr) {
        auto it = map_h.find(std::make_pair(id_ind, id));
        if (it != map_h.end())
            result = lina::AddVectors(result, it->second);
    }
    return result;
}

void ExportS0(const std::string& db_in, const std::string& table_in, const std::string& db_out, int t_trunc)
{
    bench::Timer timer;
    using namespace alg;
    MyDB dbProd(db_in);

    AdamsDeg1d gen_degs;
    int1d gen_reprs;
    dbProd.load_indecomposables(table_in + "_generators", gen_reprs, gen_degs, t_trunc);
    auto map_h_dual = dbProd.load_products_h(table_in, t_trunc);

    int2d loc2glo;
    pairii1d glo2loc;
    dbProd.load_id_converter(table_in + "_generators", loc2glo, glo2loc, t_trunc);
    std::map<std::pair<int, int>, int1d> map_h;
    for (auto& [p, arr] : map_h_dual) {
        int s_i = glo2loc[p.second].first - glo2loc[p.first].first;
        for (int i : arr)
            map_h[std::make_pair(p.first, loc2glo[s_i][i])].push_back(p.second);
    }

    std::map<AdamsDeg, Poly1d> gb;
    Mon2d leads;
    std::map<AdamsDeg, Mon1d> basis;
    std::map<AdamsDeg, int2d> repr;

    /* Add new basis */
    basis[AdamsDeg{0, 0}].push_back(Mon());
    repr[AdamsDeg{0, 0}].push_back({0});

    /* Add new basis */
    for (int t = 1; t <= t_trunc; t++) {
        std::map<AdamsDeg, Mon1d> basis_new;
        std::map<AdamsDeg, int2d> repr_new;

        /* Consider all possible basis in degree t */
        for (size_t gen_id = gen_degs.size(); gen_id-- > 0;) {
            auto repr_g = gen_reprs[gen_id];
            int t1 = t - gen_degs[gen_id].t;
            if (t1 >= 0) {
                auto p1 = basis.lower_bound(AdamsDeg{0, t1});
                auto p2 = basis.lower_bound(AdamsDeg{0, t1 + 1});
                for (auto p = p1; p != p2; ++p) {
                    for (size_t i = 0; i < p->second.size(); ++i) {
                        Mon& m = p->second[i];
                        auto& repr_m = repr.at(p->first)[i];
                        if (!m || (int)gen_id <= m[0].g()) {
                            Mon mon = m * Mon::Gen((uint32_t)gen_id);
                            auto repr_mon = mul(map_h, repr_g, repr_m);
                            auto deg_mon = p->first + gen_degs[gen_id];
                            if (gen_id >= leads.size() || std::none_of(leads[gen_id].begin(), leads[gen_id].end(), [&mon](const Mon& _m) { return divisible(_m, mon); })) {
                                basis_new[deg_mon].push_back(std::move(mon));
                                repr_new[deg_mon].push_back(std::move(repr_mon));
                            }
                        }
                    }
                }
            }
        }

        /* Compute groebner and basis in degree t */
        for (auto it = basis_new.begin(); it != basis_new.end(); ++it) {
            auto indices = ut::size_t_range(it->second.size());
            std::sort(indices.begin(), indices.end(), [&it](size_t a, size_t b) { return it->second[a] < it->second[b]; });
            Mon1d basis_new_d;
            int2d repr_new_d;
            for (size_t i = 0; i < indices.size(); ++i) {
                basis_new_d.push_back(it->second[indices[i]]);
                repr_new_d.push_back(repr_new.at(it->first)[indices[i]]);
            }

            int2d image, kernel, g;
            lina::SetLinearMap(repr_new_d, image, kernel, g);
            int1d lead_kernel;

            /* Add to groebner */
            for (const int1d& k : kernel) {
                lead_kernel.push_back(k[0]);
                gb[it->first].push_back(Indices2Poly(k, basis_new_d));
                auto& poly = gb.at(it->first).back();
                int index = poly.GetLead().front().g();
                if (leads.size() <= (size_t)index)
                    leads.resize(size_t(index + 1));
                leads[index].push_back(poly.GetLead());
            }

            /* Add to basis_H */
            std::sort(lead_kernel.begin(), lead_kernel.end());
            int1d index_basis = lina::AddVectors(ut::int_range(int(basis_new_d.size())), lead_kernel);
            for (int i : index_basis) {
                basis[it->first].push_back(std::move(basis_new_d[i]));
                repr[it->first].push_back(std::move(repr_new_d[i]));
            }
        }
    }

    /* Check basis size */
    size_t size_basis = 0;
    for (auto it = basis.begin(); it != basis.end(); ++it)
        size_basis += it->second.size();
    if (size_basis != glo2loc.size()) {
        std::cout << "size_basis=" << size_basis << '\n';
        std::cout << "glo2loc.size()=" << glo2loc.size() << '\n';
        throw MyException(0x8334a2c1U, "Error: sizes of bases do not match.");
    }

    /* Save the results */
    MyDB dbE2(db_out);
    const std::string table_out = "S0_AdamsE2";
    dbE2.drop_and_create_generators(table_out);
    dbE2.drop_and_create_relations(table_out);
    dbE2.drop_and_create_basis(table_out);

    dbE2.begin_transaction();

    dbE2.save_generators(table_out, gen_degs, gen_reprs);
    dbE2.save_gb(table_out, gb);
    dbE2.save_basis(table_out, basis, repr);

    dbE2.end_transaction();
}

void Export2Cell(const std::string& complex_name, int t_trunc)
{
    bench::Timer timer;
    using namespace alg;
    const std::string db_in = complex_name + "_Adams_chain.db";
    const std::string table_in = complex_name + "_Adams";
    const std::string db_S0 = "S0_AdamsSS.db";
    const std::string table_S0 = "S0_AdamsE2";
    const std::string db_out = complex_name + "_AdamsSS.db";
    const std::string table_out = complex_name + "_AdamsE2";

    MyDB dbProd(db_in);
    AdamsDeg1d v_degs;
    int1d gen_reprs;
    dbProd.load_indecomposables(table_in + "_E2", gen_reprs, v_degs, t_trunc);
    auto map_h_dual = dbProd.load_cell_products_h(table_in, t_trunc);
    std::map<std::pair<int, int>, int1d> map_h;
    for (auto& [p, arr] : map_h_dual) {
        int id_ind_mod = p.first;
        int id_mod = p.second;
        for (int i : arr)
            map_h[std::make_pair(id_ind_mod, i)].push_back(id_mod);
    }

    MyDB dbS0(db_S0);
    auto S0_basis = dbS0.load_basis(table_S0);
    auto S0_basis_repr = dbS0.load_basis_repr(table_S0);

    std::map<AdamsDeg, Mod1d> gbm;
    Mon2d leads;
    std::map<AdamsDeg, MMod1d> basis;
    std::map<AdamsDeg, int2d> repr;

    /* Add new basis */
    for (int t = 0; t <= t_trunc; ++t) {
        std::map<AdamsDeg, MMod1d> basis_new;
        std::map<AdamsDeg, int2d> repr_new;

        /* Consider all possible basis in degree t */
        for (size_t gen_id = v_degs.size(); gen_id-- > 0;) {
            auto repr_g = gen_reprs[gen_id];
            int t1 = t - v_degs[gen_id].t;
            if (t1 >= 0) {
                auto p1 = S0_basis.lower_bound(AdamsDeg{0, t1});
                auto p2 = S0_basis.lower_bound(AdamsDeg{0, t1 + 1});
                for (auto p = p1; p != p2; ++p) {
                    for (size_t i = 0; i < p->second.size(); ++i) {
                        auto deg_mon = p->first + v_degs[gen_id];
                        MMod m = MMod(p->second[i], (uint32_t)gen_id);
                        if (size_t(gen_id) >= leads.size() || std::none_of(leads[gen_id].begin(), leads[gen_id].end(), [&m](const Mon& _m) { return divisible(_m, m.m); })) {
                            basis_new[deg_mon].push_back(m);
                            auto repr_mon = mul(map_h, repr_g, S0_basis_repr.at(p->first)[i]);
                            repr_new[deg_mon].push_back(std::move(repr_mon));
                        }
                    }
                }
            }
        }

        /* Compute groebner and basis in degree t */
        for (auto it = basis_new.begin(); it != basis_new.end(); ++it) {
            auto indices = ut::size_t_range(it->second.size());
            std::sort(indices.begin(), indices.end(), [&it](size_t a, size_t b) { return it->second[a] < it->second[b]; });
            MMod1d basis_new_d;
            int2d repr_new_d;
            for (size_t i = 0; i < indices.size(); ++i) {
                basis_new_d.push_back(it->second[indices[i]]);
                repr_new_d.push_back(repr_new.at(it->first)[indices[i]]);
            }

            int2d image, kernel, g;
            lina::SetLinearMap(repr_new_d, image, kernel, g);
            int1d lead_kernel;

            /* Add to groebner */
            for (const int1d& k : kernel) {
                lead_kernel.push_back(k[0]);
                gbm[it->first].push_back(Indices2Mod(k, basis_new_d));
                auto& x = gbm.at(it->first).back();
                size_t index = (size_t)x.GetLead().v;
                if (leads.size() <= index)
                    leads.resize(index + 1);
                leads[index].push_back(x.GetLead().m);
            }

            /* Add to basis_H */
            std::sort(lead_kernel.begin(), lead_kernel.end());
            int1d index_basis = lina::AddVectors(ut::int_range(int(basis_new_d.size())), lead_kernel);
            for (int i : index_basis) {
                basis[it->first].push_back(std::move(basis_new_d[i]));
                repr[it->first].push_back(std::move(repr_new_d[i]));
            }
        }
    }

    size_t size_basis = 0;
    for (auto it = basis.begin(); it != basis.end(); ++it) {
        size_basis += it->second.size();
    }
    int size_E2 = dbProd.get_int("SELECT count(*) from " + table_in + "_E2 WHERE t<=" + std::to_string(t_trunc));
    if (size_basis != size_t(size_E2)) {
        std::cout << "size_basis=" << size_basis << '\n';
        std::cout << "size_E2=" << size_E2 << '\n';
        throw MyException(0x571c8119U, "Error: sizes of bases do not match.");
    }

    /* Save the results */
    MyDB dbE2(db_out);
    dbE2.drop_and_create_generators(table_out);
    dbE2.drop_and_create_relations(table_out);
    dbE2.drop_and_create_basis(table_out);

    dbE2.begin_transaction();

    dbE2.save_generators(table_out, v_degs, gen_reprs);
    dbE2.save_gb_mod(table_out, gbm);
    dbE2.save_basis_mod(table_out, basis, repr);

    dbE2.end_transaction();
}

int main_export(int argc, char** argv, int index)
{
    int t_max = 0;
    std::string db_in = "S0_Adams_res_prod.db";
    std::string table_in = "S0_Adams_res";
    std::string db_out = "S0_AdamsSS.db";

    if (argc > index + 1 && strcmp(argv[size_t(index + 1)], "-h") == 0) {
        std::cout << "Usage:\n  Adams export <t_max> [db_in] [table_in] [db_out]\n\n";

        std::cout << "Default values:\n";
        std::cout << "  db_in = " << db_in << "\n";
        std::cout << "  table_in = " << table_in << "\n";
        std::cout << "  db_out = " << db_out << "\n";
        return 0;
    }

    if (myio::load_arg(argc, argv, ++index, "t_max", t_max))
        return index;
    if (myio::load_op_arg(argc, argv, ++index, "db_in", db_in))
        return index;
    if (myio::load_op_arg(argc, argv, ++index, "table_in", table_in))
        return index;
    if (myio::load_op_arg(argc, argv, ++index, "db_out", db_out))
        return index;

    ExportS0(db_in, table_in, db_out, t_max);
    return 0;
}

int main_2cell_export(int argc, char** argv, int index)
{
    int t_trunc = 100;
    std::string complex_name;
    int t_max = 0;

    if (argc > index + 1 && strcmp(argv[size_t(index + 1)], "-h") == 0) {
        std::cout << "Usage:\n  Adams 2cell export <complex_name> <t_max>\n\n";
        std::cout << "complex_name=C2/Ceta/Cnu/Csigma\n\n";

        std::cout << "Version:\n  2.0 (2022-09-13)" << std::endl;
        return 0;
    }

    if (myio::load_arg(argc, argv, ++index, "complex_name", complex_name))
        return index;
    if (myio::load_arg(argc, argv, ++index, "t_max", t_max))
        return index;

    Export2Cell(complex_name, t_max);
    return 0;
}