.pragma library

// Small Markdown -> Qt rich text (HTML subset) converter for chat messages.
// Qt 5.6 has no Text.MarkdownText. Handles: fenced code blocks, inline code,
// headings, bold, italic, strikethrough, links, autolinks, bullet/numbered
// lists (one nesting level by indent), block quotes, rules and pipe tables.
// opts: { codeBg, codeColor, linkColor, quoteColor, ruleColor } (CSS colours)

function escapeHtml(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;")
}

// Inline markup on already escaped text. Code spans are cut out first so
// their content is left alone.
function inline(text, o) {
    var codes = []
    text = text.replace(/`([^`]+)`/g, function(m, c) {
        codes.push("<code style=\"background-color:" + o.codeBg + "; color:" + o.codeColor + "\">&nbsp;"
                   + c + "&nbsp;</code>")
        return "\u0000" + (codes.length - 1) + "\u0000"
    })
    text = text.replace(/\[([^\]]+)\]\((https?:\/\/[^)\s]+)\)/g, function(m, label, url) {
        return "<a href=\"" + url + "\" style=\"color:" + o.linkColor + "\">" + label + "</a>"
    })
    text = text.replace(/(^|[\s(])(https?:\/\/[^\s<)]+)/g, function(m, pre, url) {
        return pre + "<a href=\"" + url + "\" style=\"color:" + o.linkColor + "\">" + url + "</a>"
    })
    text = text.replace(/\*\*\*([^*]+)\*\*\*/g, "<b><i>$1</i></b>")
    text = text.replace(/\*\*([^*]+)\*\*/g, "<b>$1</b>")
    text = text.replace(/__([^_]+)__/g, "<b>$1</b>")
    text = text.replace(/(^|[^*\w])\*([^*\s][^*]*)\*(?!\*)/g, "$1<i>$2</i>")
    text = text.replace(/(^|[^_\w])_([^_\s][^_]*)_(?![_\w])/g, "$1<i>$2</i>")
    text = text.replace(/~~([^~]+)~~/g, "<s>$1</s>")
    text = text.replace(/\u0000(\d+)\u0000/g, function(m, i) { return codes[parseInt(i, 10)] })
    return text
}

function isTableSep(line) {
    return /^\s*\|?\s*:?-{2,}:?\s*(\|\s*:?-{2,}:?\s*)*\|?\s*$/.test(line)
}

function splitRow(line) {
    var s = line.trim()
    if (s.charAt(0) === "|") s = s.substring(1)
    if (s.charAt(s.length - 1) === "|") s = s.substring(0, s.length - 1)
    return s.split("|").map(function(c) { return c.trim() })
}

// Converted text is kept (by text and colours), so rows that are redrawn
// or recreated while scrolling do not convert the same Markdown again.
var memo = {}
var memoSize = 0
function toHtml(md, opts) {
    var key = JSON.stringify(opts || {}) + "\u0001" + (md || "")
    if (memo.hasOwnProperty(key))
        return memo[key]
    var html = convert(md, opts)
    if (memoSize > 400) { memo = {}; memoSize = 0 }
    memo[key] = html
    memoSize++
    return html
}

function convert(md, opts) {
    var o = opts || {}
    o.codeBg = o.codeBg || "#33000000"
    o.codeColor = o.codeColor || "#000000"
    o.linkColor = o.linkColor || "#4aa3ff"
    o.quoteColor = o.quoteColor || "#888888"
    o.ruleColor = o.ruleColor || "#888888"

    var lines = (md || "").replace(/\r\n/g, "\n").split("\n")
    var out = []
    var para = []
    var list = null     // { type: "ul"|"ol", items: [] }
    var i = 0

    function flushPara() {
        if (para.length) {
            out.push("<p>" + para.map(function(l) { return inline(escapeHtml(l), o) }).join("<br>") + "</p>")
            para = []
        }
    }
    function flushList() {
        if (list) {
            out.push("<" + list.type + ">" + list.items.join("") + "</" + list.type + ">")
            list = null
        }
    }
    function flushAll() { flushPara(); flushList() }

    while (i < lines.length) {
        var line = lines[i]
        var m

        // Fenced code block (an unclosed one runs to the end: streaming).
        if ((m = /^\s*(```|~~~)\s*([\w+#.-]*)\s*$/.exec(line))) {
            flushAll()
            var fence = m[1]
            var lang = m[2]
            var code = []
            i++
            while (i < lines.length && lines[i].trim() !== fence) {
                code.push(lines[i])
                i++
            }
            i++
            out.push("<table width=\"100%\" cellpadding=\"8\" style=\"background-color:" + o.codeBg + "\"><tr><td>"
                     + (lang ? "<small style=\"color:" + o.quoteColor + "\">" + escapeHtml(lang) + "</small><br>" : "")
                     + "<pre style=\"color:" + o.codeColor + "\">" + escapeHtml(code.join("\n")) + "</pre></td></tr></table>")
            continue
        }

        if (/^\s*$/.test(line)) {
            flushAll()
            i++
            continue
        }

        if ((m = /^(#{1,6})\s+(.*?)\s*#*\s*$/.exec(line))) {
            flushAll()
            var level = Math.min(m[1].length + 2, 6)   // # -> h3: chat-sized headings
            out.push("<h" + level + ">" + inline(escapeHtml(m[2]), o) + "</h" + level + ">")
            i++
            continue
        }

        if (/^\s*([-*_])(\s*\1){2,}\s*$/.test(line)) {
            flushAll()
            out.push("<hr style=\"color:" + o.ruleColor + "\">")
            i++
            continue
        }

        // Pipe table: header row followed by a separator row.
        if (line.indexOf("|") >= 0 && i + 1 < lines.length && isTableSep(lines[i + 1])) {
            flushAll()
            var head = splitRow(line)
            var rows = []
            i += 2
            while (i < lines.length && lines[i].indexOf("|") >= 0 && !/^\s*$/.test(lines[i])) {
                rows.push(splitRow(lines[i]))
                i++
            }
            var t = "<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\" style=\"border-color:" + o.ruleColor + "\"><tr>"
            head.forEach(function(c) { t += "<th>" + inline(escapeHtml(c), o) + "</th>" })
            t += "</tr>"
            rows.forEach(function(r) {
                t += "<tr>"
                r.forEach(function(c) { t += "<td>" + inline(escapeHtml(c), o) + "</td>" })
                t += "</tr>"
            })
            out.push(t + "</table>")
            continue
        }

        if ((m = /^\s*>\s?(.*)$/.exec(line))) {
            flushAll()
            var quote = []
            while (i < lines.length && (m = /^\s*>\s?(.*)$/.exec(lines[i]))) {
                quote.push(inline(escapeHtml(m[1]), o))
                i++
            }
            out.push("<table cellpadding=\"6\"><tr><td style=\"color:" + o.quoteColor + "\"><i>"
                     + quote.join("<br>") + "</i></td></tr></table>")
            continue
        }

        if ((m = /^(\s*)([-*+]|\d+[.)])\s+(.*)$/.exec(line))) {
            flushPara()
            var type = /\d/.test(m[2]) ? "ol" : "ul"
            var nested = m[1].length >= 2 && list
            var item = inline(escapeHtml(m[3]), o)
            if (nested && list.items.length) {
                // Sub item: append a small list to the previous item.
                var last = list.items.pop().replace(/<\/li>$/, "")
                list.items.push(last + "<" + type + "><li>" + item + "</li></" + type + "></li>")
            } else {
                if (list && list.type !== type) flushList()
                if (!list) list = { type: type, items: [] }
                list.items.push("<li>" + item + "</li>")
            }
            i++
            continue
        }

        // Continuation of a list item (indented text) or a plain paragraph line.
        if (list && /^\s{2,}\S/.test(line)) {
            var prev = list.items.pop().replace(/<\/li>$/, "")
            list.items.push(prev + "<br>" + inline(escapeHtml(line.trim()), o) + "</li>")
            i++
            continue
        }
        flushList()
        para.push(line)
        i++
    }
    flushAll()
    return out.join("")
}
