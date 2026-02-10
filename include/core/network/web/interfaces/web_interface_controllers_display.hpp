#pragma once

    String displaySlotsHtml_() const
    {
        String html;
        html.reserve(2048);
        const size_t count = _configs_manager ? _configs_manager->displaySlotCount() : 8;
        const size_t total = (count > 0) ? count : 8;
        for (size_t i = 0; i < total && i < 8; ++i)
        {
            DisplaySlotConfig slot{};
            if (_configs_manager)
                _configs_manager->displaySlot(i, slot);
            const uint8_t row = (uint8_t)(i / 4);
            const uint8_t col = (uint8_t)(i % 4);
            const String idx = String((unsigned)i);
            html += "<div class=\"slot display-slot\" data-kind=\"";
            html += displaySlotKindName_(slot.kind);
            html += "\" data-index=\"";
            html += String((unsigned)slot.index);
            html += "\" data-field=\"";
            html += displaySlotFieldName_(slot.field);
            html += "\" data-node=\"";
            html += String((unsigned long)slot.node_id);
            html += "\">";
            html += "<div class=\"slot-head\">L";
            html += String((unsigned)(row + 1));
            html += "-";
            html += String((unsigned)(col + 1));
            html += "</div>";
            html += "<label>Источник</label><select class=\"field slot-kind\" name=\"ds";
            html += idx;
            html += "_kind\"></select>";
            html += "<label>Устройство</label><select class=\"field slot-node\" name=\"ds";
            html += idx;
            html += "_node\"></select>";
            html += "<label>Объект</label><select class=\"field slot-index\" name=\"ds";
            html += idx;
            html += "_index\"></select>";
            html += "<label>Параметр</label><select class=\"field slot-field\" name=\"ds";
            html += idx;
            html += "_field\"></select>";
            html += "<label>Текст</label><input class=\"field slot-text\" type=\"text\" name=\"ds";
            html += idx;
            html += "_text\" maxlength=\"4\" value=\"";
            if (slot.text[0])
                appendHtmlEscaped_(html, slot.text);
            html += "\"></div>";
        }
        return html;
    }


    String displayDeviceOptionsJson_() const
    {
        String out;
        out.reserve(256);
        out += "[";
        bool first = true;
        auto append = [&](const String &value, const String &label) {
            if (!first)
                out += ",";
            out += "{\"v\":";
            out += value;
            out += ",\"l\":\"";
            appendJsonEscaped_(out, label);
            out += "\"}";
            first = false;
        };
        append("0", "local");
        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t id = _stack_master->nodeIdAt(i);
                String name = _stack_master->nodeNameAt(i);
                const String label = name.length() ? name : stackNodeIdHex_(id);
                append(String((unsigned long)id), label);
            }
        }
        out += "]";
        return out;
    }


    String displaySocketOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const SocketController &sockets = _controllers->sockets();
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = sockets.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Socket #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackSocketsCache *cache = stackCache().socketsCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestSockets(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        appendJsonEscaped_(list, it.name);
                    else
                        list += String("Socket #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }


    String displayLightOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const SocketController &sockets = _controllers->sockets();
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                const auto *cfg = sockets.lightConfigByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Light #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackLightsCache *cache = stackCache().lightsCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestLights(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        appendJsonEscaped_(list, it.name);
                    else
                        list += String("Light #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }


    String displayMeteoOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const MeteoController &meteo = _controllers->meteo();
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = meteo.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Sensor #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackMeteoCache *cache = stackCache().meteoCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestMeteo(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        appendJsonEscaped_(list, it.name);
                    else
                        list += String("Sensor #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }


    String displayThermoOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const ThermoController &thermo = _controllers->thermo();
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = thermo.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Thermo #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackThermoCache *cache = stackCache().thermoCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestThermo(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        appendJsonEscaped_(list, it.name);
                    else
                        list += String("Thermo #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }


    String displayTankOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const TankController &tanks = _controllers->tanks();
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = tanks.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Tank #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackTankCache *cache = stackCache().tanksCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestTanks(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        appendJsonEscaped_(list, it.name);
                    else
                        list += String("Tank #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }


    String displaySepticOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const SepticController &septic = _controllers->septic();
            for (size_t i = 0; i < SepticController::kSepticCount; ++i)
            {
                const auto *cfg = septic.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Septic #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackSepticCache *cache = stackCache().septicCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestSeptic(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.id)
                        list += String("Septic #") + String((unsigned)it.id);
                    else
                        list += String("Septic");
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }


    String displayAvrOptionsJson_() const
    {
        String out;
        out.reserve(256);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        append_node("0", "[{\"v\":1,\"l\":\"AVR\"}]");

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackAvrCache *cache = stackCache().avrCache(node_id);
                if (!cache || !cache->has_data)
                {
                    const_cast<StackCache *>(_stack_cache)->requestAvr(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                append_node(String((unsigned long)node_id), "[{\"v\":1,\"l\":\"AVR\"}]");
            }
        }
        out += "}";
        return out;
    }


    String displayLeakOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const LeakController &leak = _controllers->leak();
            for (size_t i = 0; i < LeakController::kZoneCount; ++i)
            {
                const auto *cfg = leak.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Leak #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackLeakCache *cache = stackCache().leakCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestLeak(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        appendJsonEscaped_(list, it.name);
                    else
                        list += String("Leak #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }

