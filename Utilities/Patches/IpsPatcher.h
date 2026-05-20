#pragma once

#include "pch.h"

class IpsPatcher
{
public:
	static bool PatchBuffer(string ipsFilepath, vector<uint8_t> &input, vector<uint8_t> &output);
	static bool PatchBuffer(vector<uint8_t>& ipsData, vector<uint8_t>& input, vector<uint8_t>& output);
	static bool PatchBuffer(std::istream &ipsFile, vector<uint8_t> &input, vector<uint8_t> &output);
	static vector<uint8_t> CreatePatch(vector<uint8_t> originalData, vector<uint8_t> newData);
	static vector<uint8_t> CreatePatchFrom0xFF(const vector<uint8_t>& newData);
	static bool PatchBufferAgainst0xFF(vector<uint8_t>& ipsData, size_t outputSize, vector<uint8_t>& output);
};