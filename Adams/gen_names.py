"""doc"""
import argparse
import os
import subprocess
import sqlite3
from collections import defaultdict

gen_names = {
0: "h_0",
1: "h_1",
2: "h_2",
3: "h_3",
4: "c_0",
5: "Ph_1",
6: "Ph_2",
7: "h_4",
8: "d_0",
9: "e_0",
10: "[f_0]",
11: "c_1",
12: "Pc_0",
13: "g",
14: "P^2h_1",
140: "P^7h_2",
15: "P^2h_2",
159: "P^7c_0",
16: "Pd_0",
165: "P^8h_1",
17: "i",
175: "P^8h_2",
18: "h_5",
19: "Pe_0",
20: "j",
205: "P^8c_0",
21: "P^2c_0",
22: "k",
23: "(\\Delta h_2^2)",
24: "n",
25: "d_1",
26: "p",
27: "P^3h_1",
28: "(\\Delta h_1h_3)",
29: "l",
30: "P^3h_2",
31: "P^2d_0",
32: "m",
324: "h_7",
33: "t",
34: "x",
38: "c_2",
39: "P^2e_0",
40: "Pj",
41: "P^3c_0",
42: "(\\Delta h_1d_0)",
43: "g_2",
44: "P^4h_1",
45: "(\\Delta h_0^2e_0)",
46: "(\\Delta h_1e_0)",
47: "P^4h_2",
48: "M h_1",
49: "P^3d_0",
50: "P^2i",
51: "(\\Delta h_1g)",
52: "(\\Delta h_2c_1)",
54: "C",
55: "P^3e_0",
56: "P^2j",
57: "D_1",
58: "P^4c_0",
59: "(\\Delta h_0^2i + P\\Delta h_1d_0)",
60: "(\\Delta h_0^2i)",
61: "(\\Delta_1h_1^2)",
62: "P^5h_1",
63: "P(\\Delta h_1e_0)",
64: "MP",
65: "P^5h_2",
67: "Q_2",
68: "D_2",
69: "h_6",
70: "D_3",
73: "Mh_4",
74: "(A+A^\\prime)",
75: "A",
79: "Md_0",
81: "(\\Delta x)",
82: "(\\Delta e_1+C_0+h_0^6h_5^2)",
83: "(C_0+h_0^6h_5^2)",
84: "X_2",
85: "(C^\\prime+X_2)",
86: "A^{\\prime\\prime}",
87: "P^5c_0",
91: "(\\Delta_1h_3^2)",
92: "Q_3",
93: "n_1",
94: "d_2",
96: "p^\\prime",
97: "P^6h_1",
99: "p_1",
123: "P^6c_0",
115: "P(A+A^\\prime)",
119: "(\\Delta^2h_1g)",
100: "P^2(\\Delta h_1e_0)",
102: "P^6h_2",
108: "D_3^\\prime",
131: "x_1",
132: "e_2",
135: "P^7h_1",
105: "C^{\\prime\\prime}",
114: "(\\Delta^2c_1)",
101: "Mg",
104: "D_2^\\prime",
103: "(B_5+D_2^\\prime)",
125: "P^3(\\Delta h_1d_0)",
136: "P^3(\\Delta h_1e_0)",
153: "P^5j",
152: "P^6e_0",
145: "P^6d_0",
161: "P^4(\\Delta h_1d_0)",
106: "X_3",
142: "[f_2]",
151: "(\\Delta^2d_1)",
155: "(\\Delta^2p)",
130: "t_1",
143: "c_3",
124: "[\\Delta^2h_4c_0]",
118: "[\\Delta^2h_0g]",
164: "(\\Delta h_1H_1)",
170: "(\\Delta^2e_1)",
80: "[B_4]",
98: "[\\Delta^2h_1h_4]",
186: "[M(\\Delta h_1e_0])",
178: "(\\Delta^2c_2)",
179: "(\\Delta \\Delta_1h_3^2)",
187: "M^2",
197: "(\\Delta_1g_2)",
204: "M_1h_2",
212: "[\\Delta\\Delta_1g]",
163: "g_3",
215: "M(\\Delta^2h_1)",
220: "(\\Delta^3h_2c_1)",
219: "(\\Delta^3h_1g)",
218: "(\\Delta^3h_2^2d_0)",
232: "(\\Delta^2d_0l)",
233: "(\\Delta^3h_2^2e_0)",
247: "(\\Delta^3h_0^2i)",
246: "(\\Delta^3h_0^2i+P\\Delta^3h_1d_0)",
245: "(\\Delta^2i^2)",
258: "P(\\Delta^3h_0^2e_0)",
259: "P(\\Delta^3h_1e_0)",
248: "(\\Delta^3h_2^2g)",
231: "C_1",
239: "(\\Delta h_6f_0)",
235: "(\\Delta h_6e_0)",
37: "[f_1]",
35: "[e_1]",
168: "(\\Delta^2t)",
167: "(\\Delta^2m)",
166: "(\\Delta^2e_0^2)",
169: "(\\Delta^2x)",
194: "(\\Delta^2g^2)",
193: "(\\Delta^3h_1d_0)",
207: "(\\Delta^3h_0^2e_0)",
208: "(\\Delta^3h_1e_0)",
242: "D_{11}",
206: "P(\\Delta^2d_0e_0)",
224: "[MP^5]",
251: "(\\Delta h_6g)",
76: "[H_1]",
234: "[\\Delta^2D_1+h_0h_6d_0i]",
}

def tex_outside_delimiter(text: str, symbol: str):
    """Return if symbol appears in text and is outside any pair of delimiters including ()[]{}"""
    left, right = "([{", ")]}"
    left_minus_right = 0
    for c in text:
        if c in left:
            left_minus_right += 1
        elif c in right:
            left_minus_right -= 1
        elif c == symbol and left_minus_right == 0:
            return True
    return False

def tex_pow(base, exp: int) -> str:
    """Return base^exp in latex."""
    if type(base) != str:
        base = str(base)
    if exp == 1:
        return base
    else:
        if tex_outside_delimiter(base, "^"):
            base = "(" + base + ")"
        return f"{base}^{exp}" if len(str(exp)) == 1 else f"{base}^{{{exp}}}"

def get_mon_name(m):
    it = map(int, m.split(','))
    return "".join(tex_pow(gen_names[i], e) for i, e in zip(it, it))

def get_poly_name(p):
    coeff = '+'.join(map(get_mon_name, p.split(';')))
    if tex_outside_delimiter(coeff, "+"):
        coeff = "(" + coeff + ")"
    return '(' + coeff + R"\iota_1)"

if __name__ == "__main__":
    # parser
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--edit', action='store_true', help='open the script in vscode')
    parser.add_argument('--S0', default=R"C:\Users\lwnpk\Documents\Projects\algtop_cpp_build\bin\Release\S0_AdamsSS_t255.db", help='the database file of the spectral sequence for S0')
    parser.add_argument('--cdb', help='the database file of the spectral sequence for a cofiber')
    parser.add_argument('--cname', default="C2", help='The cofiber name')
    args = parser.parse_args()
    if args.edit:
        subprocess.Popen(f"code {__file__}", shell=True)
        os.sys.exit()

    # actions
    conn_S0 = sqlite3.connect(args.S0)
    c_S0 = conn_S0.cursor()
    if args.cdb is not None:
        conn_C = sqlite3.connect(args.cdb)
        c_C = conn_C.cursor()

    # populate `gen_names`
    sql = f"select id, s, t from S0_AdamsE2_generators"
    sid = defaultdict(int)
    for id, s, t in c_S0.execute(sql):
        if id not in gen_names:
            gen_names[id] = f"a_{{{s}, {sid[s]}}}"
        sid[s] += 1

    if args.cdb is None:
        print("output to S0")
        sql = f"Update S0_AdamsE2_generators SET name=?2 where id=?1"
        c_S0.executemany(sql, gen_names.items())
    else:
        print(f"output to {args.cname}")
        gen_names_C = {}
        sql = f"select id, to_S0 from {args.cname}_AdamsE2_generators"
        for id, toS0 in c_C.execute(sql):
            if id == 0:
                gen_names_C[id] = R"\iota_0"
            else:
                gen_names_C[id] = get_poly_name(toS0)

        sql = f"Update {args.cname}_AdamsE2_generators SET name=?2 where id=?1"
        c_C.executemany(sql, gen_names_C.items())

    c_S0.close()
    conn_S0.commit()
    conn_S0.close()
    if args.cdb is not None:
        c_C.close()
        conn_C.commit()
        conn_C.close()