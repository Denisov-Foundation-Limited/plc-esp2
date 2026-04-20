/**********************************************************************/
/*                                                                    */
/* Programmable Logic Controller for ESP microcontrollers             */
/*                                                                    */
/* Copyright (C) 2026 Denisov Foundation Limited                      */
/* License: GPLv3                                                     */
/* Written by Sergey Denisov aka LittleBuster                         */
/* Email: DenisovFoundationLtd@gmail.com                              */
/*                                                                    */
/**********************************************************************/

#pragma once

#include <Arduino.h>

#include "utils/configs_manager_iface.hpp"

template <typename ConsoleT>
class CLIGroupsT
{
public:
    explicit CLIGroupsT(ConsoleT &console) : _c(console) {}

    void printHelpEnable() const
    {
        _c._io->println(F("    show groups    - list groups"));
        _c._io->println(F("    show group <id> - group details"));
    }

    void printHelpConfigLines() const
    {
        _c._io->println(F("  Groups:"));
        _c._io->println(F("    groups                 - enter Groups context"));
    }

    void printHelpContextLines() const
    {
        _c._io->println(F("  Groups:"));
        _c._io->println(F("    show                    - list groups"));
        _c._io->println(F("    show <id>               - group details"));
        _c._io->println(F("    add <name>              - add group with sort 0"));
        _c._io->println(F("    name <id> <text>        - set group name"));
        _c._io->println(F("    sort <id> <num>         - set group sort"));
        _c._io->println(F("    delete <id>             - remove group"));
    }

    void printHelpTopic() const
    {
        _c._io->println(F("Groups commands:"));
        _c._io->println(F("  groups                 - enter Groups context"));
        printHelpContextLines();
    }

    void showGroups() const
    {
        if (!_c._configs_manager)
        {
            _c._io->println(F("Config manager missing"));
            return;
        }
        _c._io->println(F("Groups:"));
        _c._io->println(F("  id   sort  name"));
        bool any = false;
        for (size_t i = 0; i < _c._configs_manager->groupCount(); ++i)
        {
            ConfigsManagerIface::GroupConfig g;
            if (!_c._configs_manager->groupByIndex(i, g) || g.id == 0)
                continue;
            any = true;
            _c._io->print(F("  "));
            _c._io->print(String((unsigned)g.id));
            if (g.id < 10)
                _c._io->print(F("    "));
            else
                _c._io->print(F("   "));
            _c._io->print(String((unsigned)g.sort));
            if (g.sort < 10)
                _c._io->print(F("     "));
            else if (g.sort < 100)
                _c._io->print(F("    "));
            else if (g.sort < 1000)
                _c._io->print(F("   "));
            else
                _c._io->print(F("  "));
            _c._io->println(g.name);
        }
        if (!any)
            _c._io->println(F("  none"));
    }

    void showGroup(uint8_t id) const
    {
        if (!_c._configs_manager)
        {
            _c._io->println(F("Config manager missing"));
            return;
        }
        ConfigsManagerIface::GroupConfig g{};
        if (!findGroup_(id, g))
        {
            _c._io->println(F("Invalid group id"));
            return;
        }
        _c._io->println(F("Group:"));
        _c._io->println(String(F("  id: ")) + String((unsigned)g.id));
        _c._io->println(String(F("  sort: ")) + String((unsigned)g.sort));
        _c._io->println(String(F("  name: ")) + g.name);
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showGroups();
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("show "))
        {
            String tail = cmd.substring(5);
            tail.trim();
            uint16_t id = 0;
            if (!ConsoleT::parseUint_(tail, id) || id == 0 || id > 255)
                _c._io->println(F("Usage: show <id>"));
            else
                showGroup((uint8_t)id);
            _c.printPrompt_();
            return true;
        }
        if (!_c._configs_manager)
        {
            _c._io->println(F("Config manager missing"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("add "))
        {
            String name = cmd.substring(4);
            name.trim();
            if (!name.length())
                _c._io->println(F("Usage: add <name>"));
            else
            {
                const uint8_t id = _c._configs_manager->allocateGroupId();
                if (id == 0 || !_c._configs_manager->setGroup(id, name, 0))
                    _c._io->println(F("Failed"));
                else
                    _c._io->println(F("OK"));
            }
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("delete "))
        {
            String tail = cmd.substring(7);
            tail.trim();
            uint16_t id = 0;
            if (!ConsoleT::parseUint_(tail, id) || id == 0 || id > 255)
                _c._io->println(F("Usage: delete <id>"));
            else if (!_c._configs_manager->removeGroup((uint8_t)id))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("name "))
        {
            String rest = cmd.substring(5);
            rest.trim();
            const int sp = rest.indexOf(' ');
            if (sp <= 0)
            {
                _c._io->println(F("Usage: name <id> <text>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, sp);
            String name = rest.substring(sp + 1);
            id_str.trim();
            name.trim();
            uint16_t id = 0;
            ConfigsManagerIface::GroupConfig g{};
            if (!ConsoleT::parseUint_(id_str, id) || id == 0 || id > 255 || !findGroup_((uint8_t)id, g))
                _c._io->println(F("Invalid group id"));
            else if (!_c._configs_manager->setGroup((uint8_t)id, name, g.sort))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("sort "))
        {
            String rest = cmd.substring(5);
            rest.trim();
            const int sp = rest.indexOf(' ');
            if (sp <= 0)
            {
                _c._io->println(F("Usage: sort <id> <num>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, sp);
            String sort_str = rest.substring(sp + 1);
            id_str.trim();
            sort_str.trim();
            uint16_t id = 0;
            uint16_t sort = 0;
            ConfigsManagerIface::GroupConfig g{};
            if (!ConsoleT::parseUint_(id_str, id) || id == 0 || id > 255 || !findGroup_((uint8_t)id, g))
                _c._io->println(F("Invalid group id"));
            else if (!ConsoleT::parseUint_(sort_str, sort))
                _c._io->println(F("Invalid sort value"));
            else if (!_c._configs_manager->setGroup((uint8_t)id, g.name, sort))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        return false;
    }

private:
    bool findGroup_(uint8_t id, ConfigsManagerIface::GroupConfig &out) const
    {
        if (!_c._configs_manager)
            return false;
        for (size_t i = 0; i < _c._configs_manager->groupCount(); ++i)
        {
            ConfigsManagerIface::GroupConfig g;
            if (!_c._configs_manager->groupByIndex(i, g) || g.id == 0)
                continue;
            if (g.id == id)
            {
                out = g;
                return true;
            }
        }
        return false;
    }

    ConsoleT &_c;
};
