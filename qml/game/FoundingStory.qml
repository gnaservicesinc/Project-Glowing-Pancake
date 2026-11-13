import QtQuick

// Prologue staging and prose. The controller supplies authored choices and terms.
QtObject {
    required property var story
    readonly property var beats: buildBeats()

    function buildBeats() {
        const beats = []
        const hospital = "hospital"
        const street = "street"
        const library = "library"
        const workshop = "workshop"
        const birth = qsTr("CALIFORNIA  /  JANUARY 1997")
        const form = story.form
        const choices = form.choices || {}
        const previewText = story.preview || ""
        const homeArt = { poor: "street-apartments", middle: street, rich: "street-hillside" }[form.family] || street
        const foundingArt = form.legal_form === "nonprofit" ? "trust" : "company"
        const tones = {
            hospital: "slate", street: "amber", "street-apartments": "slate", "street-hillside": "olive",
            library: "jade", workshop: "plum", company: "amber", trust: "jade",
            elementary: "amber", "middle-school": "olive", "high-school": "slate",
            graduation: "amber", university: "jade", "grad-lab": "plum", military: "olive"
        }
        const sectionsMeta = {
            elementary: { label: qsTr("Elementary"), step: 1, defaultArt: "elementary", baseTone: "amber" },
            middle: { label: qsTr("Middle School"), step: 2, defaultArt: "middle-school", baseTone: "olive" },
            high: { label: qsTr("High School"), step: 3, defaultArt: "high-school", baseTone: "slate" },
            graduation: { label: qsTr("Graduation"), step: 4, defaultArt: "graduation", baseTone: "amber" },
            undergrad: { label: qsTr("Undergrad"), step: 5, defaultArt: "university", baseTone: "jade" },
            grad: { label: qsTr("Grad School"), step: 6, defaultArt: "grad-lab", baseTone: "plum" }
        }

        function resolveVisual(scene) {
            const sec = sectionsMeta[scene.section] || sectionsMeta.elementary
            let art = sec.defaultArt
            let tone = sec.baseTone
            let mood = "calm"
            let tint = ""
            const leg = form.leg
            if (scene.section === "elementary") {
                if (scene.id === "elem_playground") art = homeArt
                else if (scene.id === "elem_prodigy") { art = library; mood = "academic"; tint = "#7ebae8" }
                else if (scene.id === "elem_campaign_trail") { art = form.family === "rich" ? "street-hillside" : "elementary"; mood = "civic"; tint = "#d8b878" }
                if (leg === "genius") { tone = "slate"; mood = "academic"; tint = tint || "#7ebae8" }
                else if (leg === "charisma") { tone = "jade"; mood = "social"; tint = tint || "#8ed8bc" }
                else if (leg === "political") { tone = "plum"; mood = "civic"; tint = tint || "#d8a8dc" }
                else if (leg === "wealthy") { tone = "olive"; tint = "#d8c47a" }
                if (scene.id === "elem_classroom" && choices.elem_classroom) {
                    tint = choices.elem_classroom === "outgoing" ? "#eac37a" : "#8abfe0"
                } else if (scene.id === "elem_playground" && choices.elem_playground) {
                    mood = choices.elem_playground === "athletic" ? "athletic" : "calm"
                    tint = choices.elem_playground === "athletic" ? "#8fd6b4" : "#7dc2e8"
                } else if (scene.id === "elem_project" && choices.elem_project) {
                    tint = choices.elem_project === "impulsive" ? "#e5a66c" : "#96c9dc"
                } else if (scene.id === "elem_respect_teacher" && choices.elem_respect_teacher) {
                    tint = choices.elem_respect_teacher === "respectful" ? "#a2d8b4" : "#e0a372"
                }
                if (scene.id === "elem_sharing" && choices.elem_sharing) {
                    if (choices.elem_sharing === "keep") { tone = "clay"; mood = "turbulent"; tint = "#de7f6c" }
                    else { tint = "#e5c87b" }
                }
                if (scene.id === "elem_rule_blame" && choices.elem_rule_blame === "lie") {
                    tone = leg === "charisma" ? "jade" : "clay"; mood = leg === "charisma" ? "social" : "turbulent"
                    tint = leg === "charisma" ? "#88d4ba" : "#de786b"
                } else if (scene.id === "elem_rule_blame" && choices.elem_rule_blame === "truth") {
                    tint = "#9fd0e6"
                }
            } else if (scene.section === "middle") {
                if (scene.id === "ms_sport") {
                    art = homeArt
                    if (choices.ms_sport === "hated") { tone = "clay"; mood = "turbulent"; tint = "#d97c6d" }
                    else if (choices.ms_sport && choices.ms_sport !== "no_sport") { tone = "jade"; mood = "athletic"; tint = "#8fd6b4" }
                    else if (choices.ms_sport === "no_sport") { tint = "#90bdd8" }
                } else if (scene.id === "ms_clubs") {
                    const c = String(choices.ms_clubs || "").split(",")
                    if (c.some(v => ["robotics", "coding", "maker"].includes(v))) { art = workshop; tone = "slate"; mood = "academic"; tint = "#7cc0ea" }
                    else if (c.some(v => ["drama", "music", "art", "film"].includes(v))) { art = "middle-school"; tone = "plum"; mood = "creative"; tint = "#d89be0" }
                    else if (c.some(Boolean)) { tint = "#e3be76" }
                } else if (scene.id === "setback") {
                    art = homeArt; tone = "clay"; mood = "turbulent"; tint = "#dc8470"
                } else if (scene.id === "ms_bully" || scene.id === "ms_bully_aftermath") {
                    if (choices.ms_bully === "befriend") { tone = "jade"; mood = "social"; tint = "#7ad8b2" }
                    else if (choices.ms_bully_aftermath === "martial_arts" || choices.ms_bully === "fight_back") { tone = "clay"; mood = "turbulent"; tint = "#df8274" }
                    else if (choices.ms_bully === "ignore") { tone = "clay"; mood = "turbulent"; tint = "#df8274" }
                    else if (choices.ms_bully === "report" || choices.ms_bully_aftermath === "report_later") { tone = "slate"; mood = "calm"; tint = "#9cc8df" }
                } else if (choices[scene.id]) {
                    tint = (scene.id === "ms_bday_chain" && choices.ms_bday_chain === "step_in") ? "#e6bf78" : "#a8d2c0"
                }
            } else if (scene.section === "high") {
                if (scene.id === "code_camp" || scene.id === "network_camp") { art = library; tone = "jade"; mood = "academic"; tint = "#7cc2e8" }
                else if (scene.id === "maker_camp") { art = workshop; tone = "amber"; mood = "creative"; tint = "#e8bc79" }
                else if (scene.id === "civic_camp") { art = "high-school"; tone = "plum"; mood = "civic"; tint = "#d69fd8" }
                else if (scene.id === "hs_sport_continuation") {
                    art = "high-school"; tone = choices.hs_sport_continuation === "varsity" ? "jade" : "slate"
                    mood = choices.hs_sport_continuation === "varsity" ? "athletic" : "calm"
                    tint = choices.hs_sport_continuation === "varsity" ? "#ecc46b" : "#9dc6d8"
                } else if (scene.id === "hs_bully_continuation") {
                    tone = choices.hs_bully_continuation === "endure_hs" ? "clay" : "slate"
                    mood = choices.hs_bully_continuation === "endure_hs" ? "turbulent" : "calm"
                    tint = choices.hs_bully_continuation === "endure_hs" ? "#df7a70" : "#9bc6db"
                } else if (scene.id === "hs_drugs" || scene.id === "hs_drugs_aftermath") {
                    const heavy = choices.hs_drugs === "liked_a_lot" || choices.hs_drugs_aftermath === "did_not_stop"
                    tone = heavy ? "clay" : choices.hs_drugs_aftermath === "rehab" ? "olive" : "slate"
                    mood = heavy ? "turbulent" : "calm"
                    tint = heavy ? "#d96b75" : choices.hs_drugs === "calculated" ? "#e5b86e" : choices.hs_drugs_aftermath === "rehab" ? "#b7d18c" : "#8cc4c9"
                } else if (scene.id === "hs_identity" && choices.hs_identity) {
                    if (choices.hs_identity === "organize") { tone = "plum"; mood = "social"; tint = "#dd9dd8" }
                    else if (choices.hs_identity === "systems") { art = library; mood = "academic"; tint = "#7cbbe8" }
                    else { tint = "#86d3b6" }
                } else if (choices[scene.id]) {
                    tint = ["defend", "destroy", "make_room", "quiet"].includes(choices[scene.id]) ? "#88d4ba" : "#df8474"
                }
            } else if (scene.section === "graduation") {
                art = "graduation"
                if (choices.hs_drugs_aftermath === "did_not_stop") { tone = "clay"; mood = "turbulent"; tint = "#d96b75" }
                else if (choices.grad_choice === "college") { tone = "jade"; mood = "academic"; tint = "#89d5b5" }
                else { tone = "amber"; mood = "calm"; tint = "#e5b378" }
            } else if (scene.section === "undergrad") {
                art = choices.ug_funding === "army" ? "military" : "university"
                mood = choices.ug_funding === "army" ? "service" : "academic"
                if (choices.ug_funding === "loans") { tone = "clay"; tint = "#de8770" }
                else if (choices.ug_funding === "army") { tone = "olive"; tint = "#adc881" }
                else if (choices.ug_funding === "family_paid") { tone = "amber"; tint = "#e3c67f" }
                else { tone = "jade"; tint = choices.ug_major === "ba_cog" ? "#d89fc7" : "#7cc2e8" }
            } else if (scene.section === "grad") {
                art = "grad-lab"
                mood = choices.grad_school === "enter_market" ? "calm" : "academic"
                if (choices.genius_second_phd && choices.genius_second_phd !== "decline_second") { tone = "plum"; tint = "#e6b3f0" }
                else if (choices.grad_school === "enter_market") { tone = "amber"; tint = "#e3bd78" }
                else if (choices.grad_school === "phd_ethics") { tone = "plum"; tint = "#c9a6e8" }
                else if (choices.grad_school === "phd_compbio") { tone = "jade"; tint = "#75d9b6" }
                else { tone = "jade"; tint = "#7cb8f0" }
            }
            return { art, tone: tone || tones[art] || "slate", mood, tint }
        }

        function add(id, art, date, title, narration, prompt, kind, data) {
            const extra = data || {}
            const resolvedTone = extra.tone || tones[art] || "slate"
            beats.push(Object.assign({
                id, art, tone: resolvedTone, mood: extra.mood || "calm", tint: extra.tint || "",
                section: extra.section || "", eventKind: extra.eventKind || "",
                date, title, narration, prompt, kind
            }, extra))
        }
        function field(id, art, date, title, narration, prompt, limit) {
            add(id, art, date, title, narration, prompt, "field", { limit })
        }
        function question(id, art, date, title, narration, prompt, choices) {
            add(id, art, date, title, narration, prompt, "choice", { choices })
        }
        function option(id, name, detail) {
            return { id, name, detail }
        }

        add("opening", hospital, birth, qsTr("The first breath"),
            qsTr("Rain beads on the window of a California maternity ward. Below, streetlights turn the puddles amber."),
            "", "narration", {
                afterword: qsTr("A nurse draws the curtain around the bed. In the quiet, a newborn takes a first breath. Yours.")
            })
        add("first_name", hospital, birth, qsTr("Certificate of Birth"),
            qsTr("Your family fills in the birth record. Edit every field, then sign the keepsake."),
            "", "birth")
        add("certificate",hospital,birth,qsTr("A place in the world"),
            qsTr("A name, a date, the beginning of a life."),"","certificate")
        question("family", street, qsTr("HOME  /  1997"), qsTr("The house you come home to"),
            qsTr("By morning, the rain has passed. Your parents take you home, to the street where your earliest memories will begin."),
            qsTr("How much did your family have?"), story.families.map(row => ({
                id: row.id,
                name: row.id === "poor" ? qsTr("We counted every dollar") :
                    row.id === "middle" ? qsTr("We had enough to get by") :
                    row.id === "rich" ? qsTr("We were rich / wealthy") : row.name,
                detail: row.detail
            })))
        const advantages = {
            genius: qsTr("Learn Programming (or earn a doctorate) and run the lab's first experiments yourself—even compressing school."),
            charisma: qsTr("You can talk your way into rooms—and friendships—that would otherwise stay closed."),
            wealthy: qsTr("Family wealth gives you an immediate personal capital cushion at launch (requires rich parents)."),
            political: form.family === "rich"
                ? qsTr("Your family is already wealthy. A parent enters politics, bringing public scrutiny and connections of your own.")
                : qsTr("A parent builds a political career. By the time you turn eighteen, your family has money, connections and a life in the public eye.")
        }
        question("leg", homeArt, qsTr("GROWING UP"), qsTr("An early advantage"),
            qsTr("Years from now, people will talk about how you got your start. Some of that story begins here."),
            qsTr("What will give you a head start?"), story.legs.map(row =>
                Object.assign({}, row, { detail: advantages[row.id] || row.detail })))

        const scenes = Array.from(story.chapters)
        for (const scene of scenes) {
            const sec = sectionsMeta[scene.section] || sectionsMeta.elementary
            const vis = resolveVisual(scene)
            const year = (scene.date || "2002").slice(0, 4)
            const kindBadge = scene.eventKind === "conditional" ? qsTr("CHAIN EVENT")
                : scene.eventKind === "random" ? qsTr("LIFE EVENT") : qsTr("MILESTONE")
            const speedBadge = form.leg === "genius" && scene.date !== scene.nominalDate
                ? qsTr(" · ACCELERATED") : ""
            const eyebrow = qsTr("%1  /  %2  ·  %3%4")
                .arg(sec.label.toUpperCase()).arg(year).arg(kindBadge).arg(speedBadge)
            add("chapter-" + scene.id, vis.art, eyebrow, scene.title, scene.text,
                scene.prompt || qsTr("What do you do?"), "chapter", {
                    chapter: scene.id,
                    section: scene.section || "elementary",
                    eventKind: scene.eventKind || "static",
                    maxSelections: scene.maxSelections || 1,
                    choices: scene.choices,
                    tone: vis.tone,
                    mood: vis.mood,
                    tint: vis.tint
                })
        }

        const lateLaunch = previewText.indexOf("Delayed start:") >= 0 || choices.grad_choice === "college"
        const adultHeader = lateLaunch ? qsTr("SILICON VALLEY  /  LAUNCH EVE") : qsTr("SILICON VALLEY  /  JANUARY 2015")
        const adultBody = lateLaunch
            ? qsTr("Rival research labs opened while you were finishing your degree or service. Your savings untouched and tuition behind you, it is finally time to step into the arena.")
            : qsTr("The Valley has changed around the home you grew up in. Yesterday was your eighteenth birthday. Birthday cards still stand on your desk.")
        add("eighteen", homeArt, adultHeader, lateLaunch ? qsTr("Stepping onto the floor") : qsTr("A morning of your own"),
            adultBody, "", "narration", {
                afterword: qsTr("There is no company yet, no mission to promise. First, there is the person you have become.")
            })
        const adult = lateLaunch ? qsTr("SILICON VALLEY  /  FOUNDER PROFILE") : qsTr("SILICON VALLEY  /  AGE 18")
        question("gender",homeArt,adult,qsTr("Your identity"),
            story.recognitionTerms(),
            qsTr("What is your gender identity?"), [option("woman",qsTr("Woman"),story.genderAudience("woman")),
                option("man",qsTr("Man"),story.genderAudience("man")),option("nonbinary",qsTr("Nonbinary"),story.genderAudience("nonbinary"))])
        question("pronouns",homeArt,adult,qsTr("The introduction"),qsTr("Choose the pronouns other people will use for you."),
            qsTr("Which pronouns fit?"), [option("she/her",qsTr("She / her")),option("he/him",qsTr("He / him")),
                option("they/them",qsTr("They / them")),option("other",qsTr("Other pronouns"),qsTr("Open additional pronoun sets."))])
        question("other_pronouns",homeArt,adult,qsTr("In your own words"),qsTr("Choose the set to use in your profile and conversations."),
                qsTr("Which set fits?"),story.pronounOptions.filter(value=>!["she/her","he/him","they/them"].includes(value)).map(value=>option(value,value)))
        question("orientation",homeArt,adult,qsTr("Who you’re drawn to"),story.recognitionTerms(),
            qsTr("How do you describe your sexual orientation?"), [option("straight",qsTr("Straight")),option("gay_or_lesbian",qsTr("Gay or lesbian")),
                option("bisexual",qsTr("Bisexual")),option("pansexual",qsTr("Pansexual")),option("asexual",qsTr("Asexual")),
                option("queer",qsTr("Queer")),option("questioning",qsTr("Questioning")),option("demisexual",qsTr("Demisexual"))].map(row => Object.assign({},row,{detail:story.orientationAudience(row.id)})))
        add("appearance",homeArt,qsTr("THE DAY AFTER YOUR EIGHTEENTH BIRTHDAY"),qsTr("Your reflection"),
            qsTr("Make this person your own. Your starting look is editable, from body shape to clothes and hair."),
            qsTr("Choose any feature to customize it."),"appearance")
        add("home", homeArt, lateLaunch ? qsTr("SILICON VALLEY  /  LAUNCH DAY") : qsTr("SILICON VALLEY  /  JANUARY 2015"), "", "", "", "home")
        const reflections = {
            outgoing: qsTr("You naturally gathered classmates around one table and got the conversation moving."),
            introverted: qsTr("With quiet space of your own, you learned how to think deeply without a crowd."),
            athletic: qsTr("As the park lights came on, you headed home tired, sharp, and ready to compete again."),
            sedentary: qsTr("Quiet afternoons in front of the keyboard turned curiosity into real craft."),
            deliberate: qsTr("Checking recovery files saved your project—and cemented a habit of calm forensics."),
            impulsive: qsTr("Hard-rebooting lost two pages of work, though it sharpened your bias for fast recovery."),
            share: qsTr("Helping neighbors during the squeeze turned scarcity into trusted local ties."),
            scholarship: qsTr("Long evenings paid off: merit letters opened doors money alone could not buy."),
            venture: qsTr("Fixing neighbors’ laptops taught you unit economics on kitchen tables."),
            workforce: qsTr("You packed a bag straight for Silicon Valley while classmates headed off to freshman orientation."),
            college: qsTr("Acceptance letters arrived. While peers file early startups, you step onto campus to build depth."),
            family_paid: qsTr("Every semester cleared cleanly—no bank notes hovering over graduation day."),
            loans: qsTr("You walked away with your degree and real bank debt—no extra cash cushion, just hunger."),
            army: qsTr("Four years in uniform hardened your poise under pressure and cleared college tuition in full.")
        }
        function buildSectionTimeline() {
            const order = ["elementary", "middle", "high", "graduation", "undergrad", "grad"]
            const activeSections = new Set(scenes.map(s => s.section).filter(Boolean))
            return order.map(id => {
                const meta = sectionsMeta[id]
                const dropped = (id === "undergrad" || id === "grad") &&
                    (choices.hs_drugs_aftermath === "did_not_stop" || choices.grad_choice === "workforce")
                return {
                    id,
                    label: meta.label,
                    step: meta.step,
                    tone: meta.baseTone,
                    active: activeSections.has(id),
                    skipped: dropped
                }
            })
        }
        const timeline = buildSectionTimeline()
        const expanded = []
        for (const beat of beats) {
            if (beat.id === "other_pronouns" && story.beat !== "other_pronouns" && ["she/her", "he/him", "they/them"].includes(form.pronouns)) continue
            expanded.push(Object.assign({}, beat, { timeline }))
            if (["choice", "chapter"].includes(beat.kind)) {
                const rawSelected = beat.kind === "chapter" ? choices[beat.chapter] : beat.id === "other_pronouns" ? form.pronouns : form[beat.id]
                const tokens = (beat.maxSelections || 1) > 1
                    ? String(rawSelected || "").split(",").filter(Boolean)
                    : rawSelected ? [String(rawSelected)] : []
                const picked = tokens.map(tok => (beat.choices || []).find(row => row.id === tok)).filter(Boolean)
                if (picked.length > 1) {
                    const summary = picked.map(row => row.name + (row.detail ? " (" + row.detail + ")" : "")).join("\n")
                    expanded.push({
                        id: beat.id + "-after", art: beat.art, tone: beat.tone, mood: beat.mood, tint: beat.tint,
                        section: beat.section, timeline, date: beat.date,
                        title: picked.map(r => r.name).join(" · "),
                        narration: qsTr("Across middle school, these groups shaped your afternoons and early friendships."),
                        prompt: "", kind: "narration", afterword: summary
                    })
                } else if (picked.length === 1) {
                    const choice = picked[0]
                    const selected = choice.id
                    if (choice.detail || reflections[selected] || ["gender", "orientation", "other_pronouns"].includes(beat.id)) {
                        expanded.push({
                            id: beat.id + "-after", art: beat.art, tone: beat.tone, mood: beat.mood, tint: beat.tint,
                            section: beat.section, timeline, date: beat.date, title: choice.name,
                            narration: beat.id === "gender"
                                ? qsTr("You write it in your own words and tuck the old keepsake back into the family album.")
                                : (choice.detail || ""),
                            prompt: "", kind: "narration",
                            afterword: ["gender", "orientation"].includes(beat.id) && story.communitySupport(form.gender)
                                ? story.communitySupport(form.gender)
                                : reflections[selected] || (beat.id === "other_pronouns" ? qsTr("These are the pronouns people will use for you.") : "")
                        })
                    }
                }
            }
        }
        return expanded
    }
}
