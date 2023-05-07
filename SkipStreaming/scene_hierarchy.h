#pragma once
#include <vector>
#include <utility>
#include <algorithm>

struct scene_segment
{
	int hierarchy_node_id;
	int left_leaf_id;
	int right_leaf_id;
};

bool sort_scene_segment_by_end_time(scene_segment s1, scene_segment s2)
{
	if (s1.right_leaf_id != s2.right_leaf_id)
	{
		return s1.right_leaf_id < s2.right_leaf_id;
	}
	else if (s1.left_leaf_id != s2.left_leaf_id)
	{
		return s1.left_leaf_id < s2.left_leaf_id;
	}
	else
	{
		return s1.hierarchy_node_id < s2.hierarchy_node_id;
	}
}

std::vector<int> sh_get_decendents(int node_id, int leaf_number, const std::vector<std::pair<int, int>>& hierarchy)
{
	std::vector<int> decendents;
	if (node_id < leaf_number)
	{
		decendents.push_back(node_id);
		return decendents;
	}

	std::vector<int> ld = sh_get_decendents(hierarchy[node_id - leaf_number].first, leaf_number, hierarchy);
	std::vector<int> rd = sh_get_decendents(hierarchy[node_id - leaf_number].second, leaf_number, hierarchy);

	decendents.reserve(ld.size() + rd.size());
	decendents.insert(decendents.end(), ld.begin(), ld.end());
	decendents.insert(decendents.end(), rd.begin(), rd.end());

	return decendents;
}

scene_segment sh_get_scene_segment_by_node_id(int node_id, int leaf_number, const std::vector<std::pair<int, int>>& hierarchy)
{
	std::vector<int> d = sh_get_decendents(node_id, leaf_number, hierarchy);

	scene_segment segment;
	segment.hierarchy_node_id = node_id;

	int left_leaf_id = *std::min_element(d.begin(), d.end());
	int right_leaf_id = *std::max_element(d.begin(), d.end());

	segment.left_leaf_id = left_leaf_id;
	segment.right_leaf_id = right_leaf_id;

	return segment;
}

std::vector<int> sh_cut(int n_segment, int leaf_number, const std::vector<std::pair<int, int>>& hierarchy)
{
	int root_node_id = max(hierarchy.back().first, hierarchy.back().second) + 1;
	std::vector<int> max_heap = { root_node_id };
	std::make_heap(max_heap.begin(), max_heap.end(), std::less<int>());

	for (int i = 0; i < n_segment - 1; i++)
	{
		std::pair<int, int> node = hierarchy[max_heap[0] - leaf_number];
		max_heap.push_back(node.first);
		std::push_heap(max_heap.begin(), max_heap.end());
		max_heap.push_back(node.second);
		std::push_heap(max_heap.begin(), max_heap.end());

		std::pop_heap(max_heap.begin(), max_heap.end());
		max_heap.pop_back();
	}

	return max_heap;
}