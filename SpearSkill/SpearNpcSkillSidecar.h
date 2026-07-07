#pragma once

#include <cmath>
#include <cstdarg>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

namespace SpearSkillShared
{
	struct NpcSpearEntry
	{
		UInt32 formId;
		UInt32 level;
		float progress;
		UInt32 levelUps;
	};

	struct NpcSpearKey;
	typedef bool (*NpcSpearResolveKeyCallback)(void* context, const NpcSpearKey& key, UInt32* formId);
	typedef bool (*NpcSpearMakeKeyCallback)(void* context, UInt32 formId, NpcSpearKey* key);

	class NpcSpearStore;

	class INpcSpearBackend
	{
	public:
		virtual const char* Name() const = 0;
		virtual bool Load(NpcSpearStore& store) = 0;
		virtual bool Save(const NpcSpearStore& store) const = 0;
	};

	class EmbeddedPluginNpcSpearBackend;

	class NpcSpearStore
	{
	public:
		typedef void (*LogCallback)(void* context, const char* message);

		NpcSpearStore() :
			m_count(0),
			m_dirty(false),
			m_backend(nullptr),
			m_log(nullptr),
			m_logContext(nullptr),
			m_backendLogged(false)
		{
			std::memset(m_entries, 0, sizeof(m_entries));
		}

		void Configure(INpcSpearBackend* backend, LogCallback log, void* logContext);
		bool Load();
		bool Save();
		bool LoadAuthoritativeEmbedded();
		bool SaveEmbeddedCarrier();

		bool Has(UInt32 formId) const
		{
			return FindIndex(formId) >= 0;
		}

		bool TryGet(UInt32 formId, NpcSpearEntry* outEntry) const
		{
			const SInt32 index = FindIndex(formId);
			if (index < 0)
				return false;

			if (outEntry)
				*outEntry = m_entries[index];
			return true;
		}

		NpcSpearEntry Get(UInt32 formId, const NpcSpearEntry& fallback) const
		{
			NpcSpearEntry entry = {};
			return TryGet(formId, &entry) ? entry : fallback;
		}

		NpcSpearEntry* Find(UInt32 formId)
		{
			const SInt32 index = FindIndex(formId);
			return index >= 0 ? &m_entries[index] : nullptr;
		}

		const NpcSpearEntry* Find(UInt32 formId) const
		{
			const SInt32 index = FindIndex(formId);
			return index >= 0 ? &m_entries[index] : nullptr;
		}

		NpcSpearEntry* GetOrCreate(UInt32 formId, UInt32 fallbackLevel)
		{
			if (!formId)
				return nullptr;

			if (NpcSpearEntry* existing = Find(formId))
				return existing;

			if (!SetInternal(formId, fallbackLevel, 0.0f, 0, true))
				return nullptr;

			return Find(formId);
		}

		bool Set(UInt32 formId, const NpcSpearEntry& entry)
		{
			return SetInternal(formId, entry.level, entry.progress, entry.levelUps, true);
		}

		bool Set(UInt32 formId, UInt32 level, float progress, UInt32 levelUps)
		{
			return SetInternal(formId, level, progress, levelUps, true);
		}

		bool SetLevel(UInt32 formId, UInt32 level)
		{
			NpcSpearEntry existing = {};
			if (TryGet(formId, &existing))
				return SetInternal(formId, level, existing.progress, existing.levelUps, true);

			return SetInternal(formId, level, 0.0f, 0, true);
		}

		bool SetLoaded(UInt32 formId, UInt32 level, float progress, UInt32 levelUps)
		{
			return SetInternal(formId, level, progress, levelUps, false);
		}

		bool Remove(UInt32 formId)
		{
			const SInt32 index = FindIndex(formId);
			if (index < 0)
				return false;

			const UInt32 last = m_count - 1;
			if (static_cast<UInt32>(index) != last)
				m_entries[index] = m_entries[last];

			std::memset(&m_entries[last], 0, sizeof(m_entries[last]));
			--m_count;
			MarkDirty();
			return true;
		}

		void MarkDirty()
		{
			m_dirty = true;
		}

		void ClearDirty()
		{
			m_dirty = false;
		}

		bool IsDirty() const
		{
			return m_dirty;
		}

		void Clear()
		{
			std::memset(m_entries, 0, sizeof(m_entries));
			m_count = 0;
			m_dirty = false;
		}

		SInt32 FindIndex(UInt32 formId) const
		{
			if (!formId)
				return -1;

			for (UInt32 i = 0; i < m_count; ++i)
			{
				if (m_entries[i].formId == formId)
					return static_cast<SInt32>(i);
			}

			return -1;
		}

		UInt32 Count() const
		{
			return m_count;
		}

		const NpcSpearEntry* RawEntries() const
		{
			return m_entries;
		}

		NpcSpearEntry* RawEntries()
		{
			return m_entries;
		}

		const NpcSpearEntry& EntryAt(UInt32 index) const
		{
			return m_entries[index];
		}

		NpcSpearEntry& EntryAt(UInt32 index)
		{
			return m_entries[index];
		}

		static UInt32 ClampLevel(UInt32 level)
		{
			return level > 100 ? 100 : level;
		}

		void Log(const char* format, ...) const
		{
			if (!m_log || !format)
				return;

			char buffer[512] = {};
			va_list args;
			va_start(args, format);
			_vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
			va_end(args);
			m_log(m_logContext, buffer);
		}

	private:
		friend class EmbeddedPluginNpcSpearBackend;
		static constexpr UInt32 kMaxEntries = 4096;

		bool SetInternal(UInt32 formId, UInt32 level, float progress, UInt32 levelUps, bool markDirty)
		{
			if (!formId)
				return false;

			NpcSpearEntry entry = {};
			entry.formId = formId;
			entry.level = ClampLevel(level);
			entry.progress = progress;
			entry.levelUps = levelUps;

			const SInt32 index = FindIndex(formId);
			if (index >= 0)
			{
				m_entries[index] = entry;
				if (markDirty)
					MarkDirty();
				return true;
			}

			if (m_count >= kMaxEntries)
			{
				Log("NPC Spear store full; cannot store %08X", formId);
				return false;
			}

			m_entries[m_count++] = entry;
			if (markDirty)
				MarkDirty();
			return true;
		}

		UInt32 m_count;
		bool m_dirty;
		INpcSpearBackend* m_backend;
		LogCallback m_log;
		void* m_logContext;
		bool m_backendLogged;
		NpcSpearEntry m_entries[kMaxEntries];
	};

	struct NpcSpearKey
	{
		char sourceMod[MAX_PATH];
		UInt32 objectId;
		char editorId[128];
	};

	struct NpcSpearPayloadRow
	{
		NpcSpearKey key;
		UInt32 level;
		float progress;
		UInt32 levelUps;
	};

	struct NpcSpearPayloadStats
	{
		UInt32 carrierRecords;
		UInt32 parsedEntries;
		UInt32 resolvedEntries;
		UInt32 unresolvedEntries;
		UInt32 skippedRows;
	};

	class NpcSpearPayloadCodec
	{
	public:
		static const char* Header()
		{
			return "SPEAR_NPC_SKILLS_V1";
		}

		static const char* Signature()
		{
			return "SPEAR_NPC_SKILLS";
		}

		static UInt32 Version()
		{
			return 1;
		}

		static bool LooksLikePayload(const char* payload)
		{
			if (!payload)
				return false;

			while (*payload == '\r' || *payload == '\n' || *payload == ' ' || *payload == '\t')
				++payload;

			return std::strncmp(payload, Header(), std::strlen(Header())) == 0;
		}

		static bool Parse(const char* payload, std::vector<NpcSpearPayloadRow>& rows, NpcSpearPayloadStats* stats, NpcSpearStore::LogCallback log, void* logContext)
		{
			rows.clear();
			if (stats)
				std::memset(stats, 0, sizeof(*stats));
			if (!payload || !LooksLikePayload(payload))
				return false;

			const std::string text(payload);
			size_t offset = 0;
			UInt32 lineNumber = 0;
			bool sawHeader = false;
			while (offset <= text.size())
			{
				const size_t end = text.find_first_of("\r\n", offset);
				std::string line = end == std::string::npos ? text.substr(offset) : text.substr(offset, end - offset);
				offset = end == std::string::npos ? text.size() + 1 : end + 1;
				if (end != std::string::npos && offset < text.size() && text[end] == '\r' && text[offset] == '\n')
					++offset;

				++lineNumber;
				Trim(line);
				if (line.empty() || line[0] == '#')
					continue;

				if (!sawHeader)
				{
					if (line == Header())
					{
						sawHeader = true;
						continue;
					}

					Log(log, logContext, "embedded NPC Spear payload malformed header at line %u", lineNumber);
					if (stats)
						++stats->skippedRows;
					return false;
				}

				NpcSpearPayloadRow row = {};
				if (ParseRow(line, row))
				{
					rows.push_back(row);
					if (stats)
						++stats->parsedEntries;
				}
				else
				{
					if (stats)
						++stats->skippedRows;
					Log(log, logContext, "embedded NPC Spear payload skipped malformed row at line %u", lineNumber);
				}
			}

			return sawHeader;
		}

		static std::string Write(const std::vector<NpcSpearPayloadRow>& inputRows, UInt32 defaultLevel)
		{
			std::vector<NpcSpearPayloadRow> rows;
			rows.reserve(inputRows.size());
			for (size_t i = 0; i < inputRows.size(); ++i)
			{
				NpcSpearPayloadRow row = inputRows[i];
				row.level = NpcSpearStore::ClampLevel(row.level);
				if (!std::isfinite(row.progress) || row.progress < 0.0f)
					row.progress = 0.0f;
				if (row.level == NpcSpearStore::ClampLevel(defaultLevel) &&
					row.progress <= 0.0f &&
					row.levelUps == 0)
				{
					continue;
				}
				rows.push_back(row);
			}

			std::sort(rows.begin(), rows.end(), CompareRows);

			std::string output;
			output += Header();
			output += "\r\n\r\n# sourceMod|objectIdHex|value|progress|levelUps|editorIdEscaped\r\n";
			output += "# Legacy four-column rows sourceMod|objectIdHex|value|editorIdEscaped are accepted.\r\n\r\n";
			for (size_t i = 0; i < rows.size(); ++i)
			{
				const std::string escapedEditorId = Escape(rows[i].key.editorId);
				char line[512] = {};
				_snprintf_s(line, sizeof(line), _TRUNCATE, "%s|%06X|%u|%.6f|%u|%s\r\n",
					rows[i].key.sourceMod[0] ? rows[i].key.sourceMod : "$SELF",
					rows[i].key.objectId & 0x00FFFFFF,
					static_cast<unsigned int>(NpcSpearStore::ClampLevel(rows[i].level)),
					static_cast<double>(rows[i].progress),
					static_cast<unsigned int>(rows[i].levelUps),
					escapedEditorId.c_str());
				output += line;
			}

			return output;
		}

	private:
		static void Log(NpcSpearStore::LogCallback log, void* context, const char* format, ...)
		{
			if (!log || !format)
				return;

			char buffer[512] = {};
			va_list args;
			va_start(args, format);
			_vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
			va_end(args);
			log(context, buffer);
		}

		static void Trim(std::string& value)
		{
			const char* whitespace = " \t\r\n";
			const size_t first = value.find_first_not_of(whitespace);
			if (first == std::string::npos)
			{
				value.clear();
				return;
			}

			const size_t last = value.find_last_not_of(whitespace);
			value = value.substr(first, last - first + 1);
		}

		static bool ParseRow(const std::string& line, NpcSpearPayloadRow& row)
		{
			std::vector<std::string> parts;
			SplitFields(line, parts);
			if (parts.size() != 4 && parts.size() != 6)
				return false;

			const std::string& sourceMod = parts[0];
			const std::string& objectText = parts[1];
			const std::string& valueText = parts[2];
			if (sourceMod.empty() || objectText.empty() || valueText.empty())
				return false;

			char* objectEnd = nullptr;
			const UInt32 objectId = std::strtoul(objectText.c_str(), &objectEnd, 16);
			if (!objectEnd || *objectEnd || objectId > 0x00FFFFFF)
				return false;

			char* valueEnd = nullptr;
			const UInt32 level = std::strtoul(valueText.c_str(), &valueEnd, 10);
			if (!valueEnd || *valueEnd)
				return false;

			float progress = 0.0f;
			UInt32 levelUps = 0;
			const std::string* editorIdField = &parts[3];
			if (parts.size() == 6)
			{
				const std::string& progressText = parts[3];
				const std::string& levelUpsText = parts[4];
				if (progressText.empty() || levelUpsText.empty())
					return false;

				char* progressEnd = nullptr;
				const double parsedProgress = std::strtod(progressText.c_str(), &progressEnd);
				if (!progressEnd || *progressEnd || !std::isfinite(parsedProgress))
					return false;
				if (parsedProgress > 0.0)
					progress = static_cast<float>(parsedProgress);

				char* levelUpsEnd = nullptr;
				levelUps = std::strtoul(levelUpsText.c_str(), &levelUpsEnd, 10);
				if (!levelUpsEnd || *levelUpsEnd)
					return false;

				editorIdField = &parts[5];
			}

			const std::string editorId = Unescape(*editorIdField);
			std::memset(&row, 0, sizeof(row));
			_snprintf_s(row.key.sourceMod, sizeof(row.key.sourceMod), _TRUNCATE, "%s", sourceMod.c_str());
			row.key.objectId = objectId & 0x00FFFFFF;
			row.level = NpcSpearStore::ClampLevel(level);
			row.progress = progress;
			row.levelUps = levelUps;
			_snprintf_s(row.key.editorId, sizeof(row.key.editorId), _TRUNCATE, "%s", editorId.c_str());
			return true;
		}

		static void SplitFields(const std::string& line, std::vector<std::string>& parts)
		{
			parts.clear();
			size_t start = 0;
			while (start <= line.size())
			{
				const size_t end = line.find('|', start);
				parts.push_back(end == std::string::npos ? line.substr(start) : line.substr(start, end - start));
				if (end == std::string::npos)
					break;
				start = end + 1;
			}
		}

		static std::string Escape(const char* input)
		{
			std::string output;
			if (!input)
				return output;

			for (const char* cursor = input; *cursor; ++cursor)
			{
				switch (*cursor)
				{
				case '\\': output += "\\\\"; break;
				case '|': output += "\\p"; break;
				case '\r': output += "\\r"; break;
				case '\n': output += "\\n"; break;
				default: output += *cursor; break;
				}
			}
			return output;
		}

		static std::string Unescape(const std::string& input)
		{
			std::string output;
			for (size_t i = 0; i < input.size(); ++i)
			{
				if (input[i] != '\\' || i + 1 >= input.size())
				{
					output += input[i];
					continue;
				}

				const char escaped = input[++i];
				switch (escaped)
				{
				case '\\': output += '\\'; break;
				case 'p': output += '|'; break;
				case 'r': output += '\r'; break;
				case 'n': output += '\n'; break;
				default: output += escaped; break;
				}
			}
			return output;
		}

		static bool CompareRows(const NpcSpearPayloadRow& left, const NpcSpearPayloadRow& right)
		{
			const int sourceCompare = _stricmp(left.key.sourceMod, right.key.sourceMod);
			if (sourceCompare != 0)
				return sourceCompare < 0;
			if (left.key.objectId != right.key.objectId)
				return left.key.objectId < right.key.objectId;
			return std::strcmp(left.key.editorId, right.key.editorId) < 0;
		}
	};

	class EmbeddedPluginNpcSpearBackend : public INpcSpearBackend
	{
	public:
		typedef const char* (*ReadPayloadCallback)(void* context, bool* found);
		typedef bool (*WritePayloadCallback)(void* context, const char* payload);
		typedef NpcSpearResolveKeyCallback ResolveKeyCallback;
		typedef NpcSpearMakeKeyCallback MakeKeyCallback;
		typedef void (*MarkDirtyCallback)(void* context);

		EmbeddedPluginNpcSpearBackend() :
			m_readPayload(nullptr),
			m_writePayload(nullptr),
			m_resolveKey(nullptr),
			m_makeKey(nullptr),
			m_markDirty(nullptr),
			m_context(nullptr),
			m_defaultLevel(5),
			m_lastLoadFound(false)
		{
		}

		void Configure(ReadPayloadCallback readPayload, WritePayloadCallback writePayload, ResolveKeyCallback resolveKey, MakeKeyCallback makeKey, MarkDirtyCallback markDirty, void* context, UInt32 defaultLevel)
		{
			m_readPayload = readPayload;
			m_writePayload = writePayload;
			m_resolveKey = resolveKey;
			m_makeKey = makeKey;
			m_markDirty = markDirty;
			m_context = context;
			m_defaultLevel = NpcSpearStore::ClampLevel(defaultLevel);
		}

		const char* Name() const override
		{
			return "embedded-plugin";
		}

		bool LastLoadFoundCarrier() const
		{
			return m_lastLoadFound;
		}

		bool Load(NpcSpearStore& store) override
		{
			m_lastLoadFound = false;
			if (!m_readPayload || !m_resolveKey)
			{
				store.Log("NPC Spear store backend=embedded load entries=0 ok=0 missing callbacks");
				return false;
			}

			bool found = false;
			const char* payload = m_readPayload(m_context, &found);
			m_lastLoadFound = found;
			if (!found || !payload)
			{
				store.Log("NPC Spear embedded carrier not found");
				return false;
			}

			std::vector<NpcSpearPayloadRow> rows;
			NpcSpearPayloadStats stats = {};
			stats.carrierRecords = 1;
			if (!NpcSpearPayloadCodec::Parse(payload, rows, &stats, store.m_log, store.m_logContext))
			{
				store.Log("NPC Spear embedded carrier found but payload parse failed");
				return false;
			}

			for (size_t i = 0; i < rows.size(); ++i)
			{
				UInt32 formId = 0;
				if (!m_resolveKey(m_context, rows[i].key, &formId) || !formId)
				{
					++stats.unresolvedEntries;
					continue;
				}

				NpcSpearEntry existing = {};
				float progress = rows[i].progress;
				UInt32 levelUps = rows[i].levelUps;
				if (store.TryGet(formId, &existing))
				{
					progress = existing.progress;
					levelUps = existing.levelUps;
				}

				if (store.SetLoaded(formId, rows[i].level, progress, levelUps))
					++stats.resolvedEntries;
			}

			store.ClearDirty();
			store.Log("NPC Spear store backend=embedded load carriers=%u parsed=%u resolved=%u unresolved=%u skipped=%u",
				stats.carrierRecords,
				stats.parsedEntries,
				stats.resolvedEntries,
				stats.unresolvedEntries,
				stats.skippedRows);
			return true;
		}

		bool Save(const NpcSpearStore& store) const override
		{
			if (!m_writePayload || !m_makeKey)
			{
				store.Log("NPC Spear store backend=embedded save entries=0 ok=0 missing callbacks");
				return false;
			}

			std::vector<NpcSpearPayloadRow> rows;
			UInt32 skipped = 0;
			for (UInt32 i = 0; i < store.Count(); ++i)
			{
				const NpcSpearEntry& entry = store.EntryAt(i);
				if (!entry.formId)
					continue;

				NpcSpearPayloadRow row = {};
				if (!m_makeKey(m_context, entry.formId, &row.key))
				{
					++skipped;
					continue;
				}

				row.level = entry.level;
				row.progress = entry.progress;
				row.levelUps = entry.levelUps;
				rows.push_back(row);
			}

			const std::string payload = NpcSpearPayloadCodec::Write(rows, m_defaultLevel);
			if (!m_writePayload(m_context, payload.c_str()))
			{
				store.Log("NPC Spear store backend=embedded save entries=%u ok=0 skipped=%u", static_cast<unsigned int>(rows.size()), skipped);
				return false;
			}

			if (m_markDirty)
				m_markDirty(m_context);

			store.Log("NPC Spear store backend=embedded save entries=%u ok=1 skipped=%u", static_cast<unsigned int>(rows.size()), skipped);
			return true;
		}

	private:
		ReadPayloadCallback m_readPayload;
		WritePayloadCallback m_writePayload;
		ResolveKeyCallback m_resolveKey;
		MakeKeyCallback m_makeKey;
		MarkDirtyCallback m_markDirty;
		void* m_context;
		UInt32 m_defaultLevel;
		bool m_lastLoadFound;
	};

	inline void NpcSpearStore::Configure(INpcSpearBackend* backend, LogCallback log, void* logContext)
	{
		m_backend = backend;
		m_log = log;
		m_logContext = logContext;

		if (m_backend && !m_backendLogged)
		{
			Log("NPC Spear store backend selected: %s", m_backend->Name());
			m_backendLogged = true;
		}
	}

	inline bool NpcSpearStore::Load()
	{
		if (!m_backend)
		{
			Log("NPC Spear store has no backend for load");
			return false;
		}

		return m_backend->Load(*this);
	}

	inline bool NpcSpearStore::LoadAuthoritativeEmbedded()
	{
		return Load();
	}

	inline bool NpcSpearStore::Save()
	{
		if (!m_backend)
		{
			Log("NPC Spear store has no backend for save");
			return false;
		}

		const bool saved = m_backend->Save(*this);
		if (saved)
			ClearDirty();
		return saved;
	}

	inline bool NpcSpearStore::SaveEmbeddedCarrier()
	{
		return Save();
	}

	struct NpcSpearTrainingEntry
	{
		UInt32 formId;
	};

	class NpcSpearTrainingStore
	{
	public:
		NpcSpearTrainingStore() :
			m_count(0),
			m_dirty(false)
		{
			std::memset(m_entries, 0, sizeof(m_entries));
		}

		bool Has(UInt32 formId) const
		{
			return FindIndex(formId) >= 0;
		}

		bool TryGet(UInt32 formId, NpcSpearTrainingEntry* outEntry) const
		{
			const SInt32 index = FindIndex(formId);
			if (index < 0)
				return false;

			if (outEntry)
				*outEntry = m_entries[index];
			return true;
		}

		bool Set(UInt32 formId)
		{
			return SetInternal(formId, true);
		}

		bool SetLoaded(UInt32 formId)
		{
			return SetInternal(formId, false);
		}

		bool Remove(UInt32 formId)
		{
			const SInt32 index = FindIndex(formId);
			if (index < 0)
				return false;

			const UInt32 last = m_count - 1;
			if (static_cast<UInt32>(index) != last)
				m_entries[index] = m_entries[last];

			std::memset(&m_entries[last], 0, sizeof(m_entries[last]));
			--m_count;
			m_dirty = true;
			return true;
		}

		void Clear()
		{
			std::memset(m_entries, 0, sizeof(m_entries));
			m_count = 0;
			m_dirty = false;
		}

		void ClearDirty()
		{
			m_dirty = false;
		}

		bool IsDirty() const
		{
			return m_dirty;
		}

		UInt32 Count() const
		{
			return m_count;
		}

		const NpcSpearTrainingEntry& EntryAt(UInt32 index) const
		{
			return m_entries[index];
		}

	private:
		static constexpr UInt32 kMaxEntries = 4096;

		SInt32 FindIndex(UInt32 formId) const
		{
			if (!formId)
				return -1;

			for (UInt32 i = 0; i < m_count; ++i)
			{
				if (m_entries[i].formId == formId)
					return static_cast<SInt32>(i);
			}

			return -1;
		}

		bool SetInternal(UInt32 formId, bool markDirty)
		{
			if (!formId)
				return false;

			const SInt32 index = FindIndex(formId);
			if (index >= 0)
			{
				if (markDirty)
					m_dirty = true;
				return true;
			}

			if (m_count >= kMaxEntries)
				return false;

			m_entries[m_count++].formId = formId;
			if (markDirty)
				m_dirty = true;
			return true;
		}

		UInt32 m_count;
		bool m_dirty;
		NpcSpearTrainingEntry m_entries[kMaxEntries];
	};

	struct NpcSpearTrainingPayloadRow
	{
		NpcSpearKey key;
	};

	class NpcSpearTrainingPayloadCodec
	{
	public:
		static const char* Header()
		{
			return "SPEAR_NPC_TRAINING_V1";
		}

		static const char* Signature()
		{
			return "SPEAR_NPC_TRAINING";
		}

		static UInt32 Version()
		{
			return 1;
		}

		static bool LooksLikePayload(const char* payload)
		{
			if (!payload)
				return false;

			while (*payload == '\r' || *payload == '\n' || *payload == ' ' || *payload == '\t')
				++payload;

			return std::strncmp(payload, Header(), std::strlen(Header())) == 0;
		}

		static const char* PayloadSkillName()
		{
			return "Spear";
		}

		static bool Parse(const char* payload, std::vector<NpcSpearTrainingPayloadRow>& rows, NpcSpearPayloadStats* stats, NpcSpearStore::LogCallback log, void* logContext)
		{
			rows.clear();
			if (stats)
				std::memset(stats, 0, sizeof(*stats));
			if (!payload || !LooksLikePayload(payload))
				return false;

			const std::string text(payload);
			size_t offset = 0;
			UInt32 lineNumber = 0;
			bool sawHeader = false;
			while (offset <= text.size())
			{
				const size_t end = text.find_first_of("\r\n", offset);
				std::string line = end == std::string::npos ? text.substr(offset) : text.substr(offset, end - offset);
				offset = end == std::string::npos ? text.size() + 1 : end + 1;
				if (end != std::string::npos && offset < text.size() && text[end] == '\r' && text[offset] == '\n')
					++offset;

				++lineNumber;
				Trim(line);
				if (line.empty() || line[0] == '#')
					continue;

				if (!sawHeader)
				{
					if (line == Header())
					{
						sawHeader = true;
						continue;
					}

					Log(log, logContext, "embedded NPC Spear training payload malformed header at line %u", lineNumber);
					if (stats)
						++stats->skippedRows;
					return false;
				}

				NpcSpearTrainingPayloadRow row = {};
				if (ParseRow(line, row))
				{
					rows.push_back(row);
					if (stats)
						++stats->parsedEntries;
				}
				else
				{
					if (stats)
						++stats->skippedRows;
					Log(log, logContext, "embedded NPC Spear training payload skipped malformed row at line %u", lineNumber);
				}
			}

			return sawHeader;
		}

		static std::string Write(const std::vector<NpcSpearTrainingPayloadRow>& inputRows)
		{
			std::vector<NpcSpearTrainingPayloadRow> rows;
			rows.reserve(inputRows.size());
			for (size_t i = 0; i < inputRows.size(); ++i)
			{
				if (inputRows[i].key.objectId != 0)
					rows.push_back(inputRows[i]);
			}

			std::sort(rows.begin(), rows.end(), CompareRows);

			std::string output;
			output += Header();
			output += "\r\n\r\n# sourceMod|objectIdHex|trainingSkill|editorIdEscaped\r\n";
			output += "# trainingSkill values: Spear. Native TESAIForm::trainingSkill remains a vanilla fallback.\r\n\r\n";
			for (size_t i = 0; i < rows.size(); ++i)
			{
				const std::string escapedEditorId = Escape(rows[i].key.editorId);
				char line[512] = {};
				_snprintf_s(line, sizeof(line), _TRUNCATE, "%s|%06X|Spear|%s\r\n",
					rows[i].key.sourceMod[0] ? rows[i].key.sourceMod : "$SELF",
					rows[i].key.objectId & 0x00FFFFFF,
					escapedEditorId.c_str());
				output += line;
			}

			return output;
		}

	private:
		static void Log(NpcSpearStore::LogCallback log, void* context, const char* format, ...)
		{
			if (!log || !format)
				return;

			char buffer[512] = {};
			va_list args;
			va_start(args, format);
			_vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
			va_end(args);
			log(context, buffer);
		}

		static void Trim(std::string& value)
		{
			const char* whitespace = " \t\r\n";
			const size_t first = value.find_first_not_of(whitespace);
			if (first == std::string::npos)
			{
				value.clear();
				return;
			}

			const size_t last = value.find_last_not_of(whitespace);
			value = value.substr(first, last - first + 1);
		}

		static bool ParseRow(const std::string& line, NpcSpearTrainingPayloadRow& row)
		{
			std::vector<std::string> parts;
			SplitFields(line, parts);
			if (parts.size() != 4)
				return false;

			const std::string& sourceMod = parts[0];
			const std::string& objectText = parts[1];
			const std::string& skillText = parts[2];
			if (sourceMod.empty() || objectText.empty() || skillText.empty())
				return false;
			if (_stricmp(skillText.c_str(), "Spear") && std::strcmp(skillText.c_str(), "1"))
				return false;

			char* objectEnd = nullptr;
			const UInt32 objectId = std::strtoul(objectText.c_str(), &objectEnd, 16);
			if (!objectEnd || *objectEnd || objectId > 0x00FFFFFF)
				return false;

			const std::string editorId = Unescape(parts[3]);
			std::memset(&row, 0, sizeof(row));
			_snprintf_s(row.key.sourceMod, sizeof(row.key.sourceMod), _TRUNCATE, "%s", sourceMod.c_str());
			row.key.objectId = objectId & 0x00FFFFFF;
			_snprintf_s(row.key.editorId, sizeof(row.key.editorId), _TRUNCATE, "%s", editorId.c_str());
			return true;
		}

		static void SplitFields(const std::string& line, std::vector<std::string>& parts)
		{
			parts.clear();
			size_t start = 0;
			while (start <= line.size())
			{
				const size_t end = line.find('|', start);
				parts.push_back(end == std::string::npos ? line.substr(start) : line.substr(start, end - start));
				if (end == std::string::npos)
					break;
				start = end + 1;
			}
		}

		static std::string Escape(const char* input)
		{
			std::string output;
			if (!input)
				return output;

			for (const char* cursor = input; *cursor; ++cursor)
			{
				switch (*cursor)
				{
					case '\\': output += "\\\\"; break;
					case '|': output += "\\p"; break;
					case '\r': output += "\\r"; break;
					case '\n': output += "\\n"; break;
					default: output += *cursor; break;
				}
			}
			return output;
		}

		static std::string Unescape(const std::string& input)
		{
			std::string output;
			for (size_t i = 0; i < input.size(); ++i)
			{
				if (input[i] != '\\' || i + 1 >= input.size())
				{
					output += input[i];
					continue;
				}

				const char escaped = input[++i];
				switch (escaped)
				{
					case '\\': output += '\\'; break;
					case 'p': output += '|'; break;
					case 'r': output += '\r'; break;
					case 'n': output += '\n'; break;
					default: output += escaped; break;
				}
			}
			return output;
		}

		static bool CompareRows(const NpcSpearTrainingPayloadRow& left, const NpcSpearTrainingPayloadRow& right)
		{
			const int sourceCompare = _stricmp(left.key.sourceMod, right.key.sourceMod);
			if (sourceCompare != 0)
				return sourceCompare < 0;
			if (left.key.objectId != right.key.objectId)
				return left.key.objectId < right.key.objectId;
			return std::strcmp(left.key.editorId, right.key.editorId) < 0;
		}
	};
}
