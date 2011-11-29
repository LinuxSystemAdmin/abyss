#ifndef ASQGIO_H
#define ASQGIO_H 1

#include "Common/ContigID.h"
#include "Common/IOUtil.h"
#include "Graph/Properties.h"
#include <boost/graph/graph_traits.hpp>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

using boost::graph_traits;

/** Write a graph in ASQG format. */
template <typename Graph>
std::ostream& write_asqg(std::ostream& out, Graph& g)
{
	typedef typename graph_traits<Graph>::edge_descriptor E;
	typedef typename graph_traits<Graph>::edge_iterator Eit;
	typedef typename graph_traits<Graph>::vertex_descriptor V;
	typedef typename graph_traits<Graph>::vertex_iterator Vit;
	typedef typename vertex_bundle_type<Graph>::type VP;

	out << "HT\tVN:i:1\n";
	assert(out);

	std::pair<Vit, Vit> vrange = vertices(g);
	for (Vit uit = vrange.first; uit != vrange.second; ++uit, ++uit) {
		V u = *uit;
		if (get(vertex_removed, g, u))
			continue;
		const VP& vp = g[u];
		out << "VT\t" << ContigID(u) << "\t*\tLN:i:" << vp.length;
		if (vp.coverage > 0)
			out << "\tXC:i:" << vp.coverage;
		out << '\n';
	}

	std::pair<Eit, Eit> erange = edges(g);
	for (Eit eit = erange.first; eit != erange.second; ++eit) {
		E e = *eit;
		V u = source(e, g);
		V v = target(e, g);
		if (v < u || get(vertex_removed, g, u))
			continue;
		assert(!get(vertex_removed, g, v));
		int distance = g[e].distance;
		assert(distance < 0);
		unsigned overlap = -distance;
		unsigned ulen = g[u].length;
		unsigned vlen = g[v].length;
		out << "ED\t" << ContigID(u) << ' ' << ContigID(v)
			<< ' ' << (u.sense() ? 0 : ulen - overlap)
			<< ' ' << (u.sense() ? overlap : ulen) - 1
			<< ' ' << ulen
			<< ' ' << (!v.sense() ? 0 : vlen - overlap)
			<< ' ' << (!v.sense() ? overlap : vlen) - 1
			<< ' ' << vlen
			<< ' ' << (u.sense() != v.sense())
			<< " -1\n"; // number of mismatches
	}
	return out;
}

/** Read a graph in ASQG format. */
template <typename Graph>
std::istream& read_asqg(std::istream& in, Graph& g)
{
	assert(in);

	typedef typename graph_traits<Graph>::vertex_descriptor V;
	typedef typename graph_traits<Graph>::edge_descriptor E;
	typedef typename vertex_property<Graph>::type VP;
	typedef typename edge_property<Graph>::type EP;

	// Add vertices if this graph is empty.
	bool addVertices = num_vertices(g) == 0;

	while (in && in.peek() != EOF) {
		switch (in.peek()) {
		  case 'H':
			in >> expect("HT") >> Ignore('\n');
			assert(in);
			break;
		  case 'V': {
			std::string uname, seq;
			in >> expect("VT") >> uname >> seq;
			assert(in);
			assert(!seq.empty());

			unsigned length = 0;
			if (seq == "*") {
				in >> expect(" LN:i:") >> length;
				assert(in);
			} else
				length = seq.size();
			in >> Ignore('\n');

			if (addVertices) {
				VP vp;
				put(vertex_length, vp, length);
				V u = add_vertex(vp, g);
				put(vertex_name, g, u, uname);
			} else {
				V u(uname, false);
				assert(get(vertex_index, g, u) < num_vertices(g));
			}
			break;
		  }
		  case 'E': {
			ContigID uname, vname;
			unsigned s1, e1, l1, s2, e2, l2;
			bool rc;
			int nd;
			in >> expect("ED") >> uname >> vname
				>> s1 >> e1 >> l1
				>> s2 >> e2 >> l2
				>> rc >> nd >> Ignore('\n');
			assert(in);
			assert(s1 < e1 && e1 < l1 && s2 < e2 && e2 < l2);
			assert(e1 - s1 == e2 - s2);
			assert(e1 - s1 + 1 < l1 && e2 - s2 + 1 < l2);
			assert(((s1 > 0) == (s2 > 0)) == rc);
			int d = -(e1 - s1 + 1);
			assert(d < 0);
			EP ep(d);
			V u(uname, s1 == 0), v(vname, s2 > 0);
			std::pair<E, bool> e = edge(u, v, g);
			if (e.second) {
				// Ignore duplicate edges that are self loops.
				assert(g[e.first] == ep);
				assert(u == v);
			} else
				add_edge(u, v, ep, g);
			break;
		  }
		  default: {
			std::string s;
			in >> s;
			std::cerr << "error: unknown record type: `"
				<< s << "'\n";
			exit(EXIT_FAILURE);
		  }
		}
	}
	assert(in.eof());
	return in;
}

#endif
