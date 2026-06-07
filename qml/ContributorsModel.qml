// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
import QtQuick

QtObject {
    id: root

    readonly property var fallbackContributors: [
        {
            "name": "Herman",
            "role": i18n("Lead Developer"),
            "initials": "H",
            "email": "strandzen2@gmail.com",
            "web": "https://github.com/strandzen"
        },
        {
            "name": "Heimen Stoffels",
            "role": i18n("Dutch Translator / Contributor"),
            "initials": "HS",
            "email": "vistausss@fastmail.com",
            "web": "https://github.com/vistausss"
        }
    ]

    property var contributors: []
    property bool isLoading: false

    function getInitials(name) {
        if (!name) return "?";
        var parts = name.split(" ").filter(function(p) { return p.length > 0; });
        if (parts.length === 0) return "?";
        if (parts.length === 1) return parts[0].substring(0, Math.min(2, parts[0].length)).toUpperCase();
        return (parts[0][0] + parts[1][0]).toUpperCase();
    }

    function fetch() {
        isLoading = true;
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "https://api.github.com/repos/strandzen/AppImage-Manager/contributors", true);
        xhr.timeout = 8000;

        function handleFailure() {
            isLoading = false;
            contributors = fallbackContributors;
        }

        xhr.onerror   = handleFailure;
        xhr.ontimeout = handleFailure;

        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                isLoading = false;
                if (xhr.status === 200) {
                    try {
                        var rawList = JSON.parse(xhr.responseText);
                        var parsed = [];
                        for (var i = 0; i < rawList.length; i++) {
                            var item = rawList[i];
                            var login = item.login ? item.login.toLowerCase() : "";

                            // Exclude AI / bot / assistant accounts
                            if (login.indexOf("claude") !== -1 ||
                                login.indexOf("gemini") !== -1 ||
                                login.indexOf("antigravity") !== -1 ||
                                login.indexOf("copilot") !== -1 ||
                                login.indexOf("bot") !== -1 ||
                                login.indexOf("assistant") !== -1) {
                                continue;
                            }

                            var dispName = item.login || "Contributor";
                            parsed.push({
                                "name": dispName,
                                "role": i === 0 ? i18n("Lead Developer") : i18n("Contributor"),
                                "initials": getInitials(dispName),
                                "email": "",
                                "web": item.html_url || "https://github.com/strandzen/AppImage-Manager"
                            });
                        }

                        contributors = parsed.length > 0 ? parsed : fallbackContributors;
                    } catch (e) {
                        contributors = fallbackContributors;
                    }
                } else {
                    contributors = fallbackContributors;
                }
            }
        }
        xhr.send();
    }
}
