.pragma library

// Brings a ListModel in line with a plain array of message objects
// ({ uuid, fromUser, text, kind }) by changing only what differs: rows that
// stay the same keep their delegates, so nothing flickers or re-renders.
function sync(model, items) {
    items = items || []
    for (var i = 0; i < items.length; ++i) {
        var it = items[i]
        var row = {
            uuid: it.uuid || "",
            fromUser: !!it.fromUser,
            text: it.text || "",
            kind: it.kind || ""
        }
        if (i < model.count) {
            var cur = model.get(i)
            if (cur.text !== row.text || cur.fromUser !== row.fromUser || cur.kind !== row.kind
                    || cur.uuid !== row.uuid)
                model.set(i, row)
        } else {
            model.append(row)
        }
    }
    if (model.count > items.length)
        model.remove(items.length, model.count - items.length)
}
