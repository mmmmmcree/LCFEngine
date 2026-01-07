#pragma once

#include <ranges>
#include <boost/graph/adjacency_list.hpp>

namespace lcf::impl {
    template <typename VertexProperty, typename EdgeProperty>
    class DirectedGraph;

    template <typename VertexProperty>
    class DirectedGraph<VertexProperty, boost::no_property>
    {
        using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, VertexProperty>;
    public:
        using VertexId = typename boost::graph_traits<Graph>::vertex_descriptor;
    public:
        DirectedGraph() = default;
        ~DirectedGraph() = default;
        DirectedGraph(const DirectedGraph &) = default;
        DirectedGraph & operator=(const DirectedGraph &) = default;
        DirectedGraph(DirectedGraph &&) = default;
        DirectedGraph & operator=(DirectedGraph &&) = default;
        VertexProperty & operator[](VertexId vertex) noexcept { return m_graph[vertex]; }
        const VertexProperty & operator[](VertexId vertex) const noexcept { return m_graph[vertex]; }
        VertexId addVertex(const VertexProperty & property) noexcept { return boost::add_vertex(property, m_graph); }
        VertexId addVertex(VertexProperty && property) noexcept { return boost::add_vertex(std::move(property), m_graph); }
        void addEdge(VertexId from, VertexId to) noexcept { boost::add_edge(from, to, m_graph); }
        auto getVertices() noexcept
        {
            auto [begin, end] = boost::vertices(m_graph);
            return std::ranges::subrange(begin, end) | this->getVertexViews();
        }
        auto getVertices() const noexcept
        {
            auto [begin, end] = boost::vertices(m_graph);
            return std::ranges::subrange(begin, end) | this->getVertexViews();
        }
        auto getAdjacentVertices(VertexId vertex) noexcept
        {
            auto [begin, end] = boost::adjacent_vertices(vertex, m_graph);
            return std::ranges::subrange(begin, end) | this->getVertexViews();
        }
        auto getAdjacentVertices(VertexId vertex) const noexcept
        {
            auto [begin, end] = boost::adjacent_vertices(vertex, m_graph);
            return std::ranges::subrange(begin, end) | this->getVertexViews();
        }
        template <TraversalOrder order>
        auto traverseFrom(VertexId start) noexcept
        {
            using TraversalOrder = typename traversal_order_traits<order, Graph>::order_t;
            return TraversalOrder{}(m_graph, start) | this->getVertexViews();
        }
        template <TraversalOrder order>
        auto traverseFrom(VertexId start) const noexcept
        {
            using TraversalOrder = typename traversal_order_traits<order, Graph>::order_t;
            return TraversalOrder{}(m_graph, start) | this->getVertexViews();
        }
    private:
        auto getVertexViews() noexcept
        {
            return std::views::transform([this](auto v) { return std::make_pair(v, std::ref(m_graph[v])); });
        }
        auto getVertexViews() const noexcept
        {
            return std::views::transform([this](auto v) { return std::make_pair(v, std::cref(m_graph[v])); });
        }
    private:
        Graph m_graph;
    };
}

namespace lcf {
    template <typename VertexProperty>
    using DirectedGraphV = impl::DirectedGraph<VertexProperty, boost::no_property>;

    template <typename EdgeProperty>
    using DirectedGraphE = impl::DirectedGraph<boost::no_property, EdgeProperty>;

    template <typename VertexProperty, typename EdgeProperty>
    using DirectedGraph = impl::DirectedGraph<VertexProperty, EdgeProperty>;
}