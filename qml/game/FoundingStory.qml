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
        const founding = qsTr("THE FOUNDING  /  JANUARY 2015")
        const form = story.form
        const homeArt = { poor: "street-apartments", middle: street, rich: "street-hillside" }[form.family] || street
        const foundingArt = form.legal_form === "nonprofit" ? "trust" : "company"
        const tones = {
            hospital: "slate", street: "amber", "street-apartments": "slate", "street-hillside": "olive",
            library: "jade", workshop: "plum", company: "amber", trust: "jade"
        }

        function add(id, art, date, title, narration, prompt, kind, data) {
            beats.push(Object.assign({ id, art, tone: tones[art], date, title, narration, prompt, kind }, data || {}))
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
        field("first_name", hospital, birth, qsTr("The birth record"),
            qsTr("The nurse hands your family the birth record. They fill in your given name."),
            qsTr("Given name"), 63)
        field("last_name", hospital, birth, qsTr("The birth record"),
            qsTr("Your family adds the surname you’ll carry home."),
            qsTr("Surname"), 63)
        question("birth_sex", hospital, birth, qsTr("The birth record"),
            qsTr("The hospital records the sex observed at birth."), qsTr("Sex at birth"),
            [option("female",qsTr("Female")),option("male",qsTr("Male")),option("indeterminate",qsTr("Indeterminate"))])
        add("certificate",hospital,birth,qsTr("A place in the world"),
            qsTr("A name, a date, the beginning of a life. Your family signs the keepsake and takes it home."),"","certificate")
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
            genius: qsTr("You can take on the lab's first experiment yourself."),
            charisma: qsTr("You can talk your way into rooms that would otherwise stay closed."),
            wealthy: qsTr("Family money can fund a lab of your own."),
            political: qsTr("Your family rises into wealth and influence by the time you turn eighteen.")
        }
        question("leg", homeArt, qsTr("GROWING UP"), qsTr("An early advantage"),
            qsTr("Years from now, people will talk about how you got your start. Some of that story begins here."),
            qsTr("What will give you a head start?"), story.legs.map(row =>
                Object.assign({}, row, { detail: advantages[row.id] || row.detail })))

        const scenes = Array.from(story.chapters)
        const sceneCopy = {
            arrival: {
                art: homeArt, date: qsTr("HOME  /  1997"), title: qsTr("Stay or go"),
                narration: qsTr("A job offer arrives from another town. Your parents spend an evening at the kitchen table, weighing a new address against the people they know."),
                prompt: qsTr("What do they decide?")
            },
            curiosity: {
                art: library, title: qsTr("After school"),
                narration: qsTr("At six, you are allowed to walk from school to the library. In the back corner, an old computer hums beside the window. Outside, you can hear the neighborhood team practicing."),
                prompt: qsTr("Where do you spend your afternoons?")
            },
            setback: {
                art: homeArt, title: qsTr("The year the phones stopped ringing"),
                narration: qsTr("In 2008, a shop closes at the end of your street. Then another. Families trade rides and tools while they wait for work to come back."),
                prompt: qsTr("Where do you put your energy?")
            },
            circle: {
                art: library, title: qsTr("Finding your people"),
                narration: qsTr("By thirteen, you know who stays late at the library. One group is building for a science fair. Another is organizing a neighborhood project. Across the street, a shopkeeper could use help."),
                prompt: qsTr("Whose work do you join?")
            },
            promise: {
                art: workshop, title: qsTr("The result"),
                narration: qsTr("One evening, at a borrowed desk, an experiment works. You run it again. This time the result is less clear. Someone is already asking for a demonstration."),
                prompt: qsTr("How do you show your work?")
            },
            ownership: {
                art: workshop, title: qsTr("The price of help"),
                narration: qsTr("A visitor comes by the workshop and offers to pay for equipment. Before leaving, they ask who will own the method once it works."),
                prompt: qsTr("What will you share?")
            },
            record: {
                art: workshop, title: qsTr("On the record"),
                narration: qsTr("Three reviewers pull up chairs at your workbench. One points to an example your system handled badly and asks how often it happens."),
                prompt: qsTr("How do you answer the panel?")
            },
            threshold: {
                art: workshop, title: qsTr("Eighteen"),
                narration: form.leg === "political" && form.family !== "rich"
                    ? qsTr("Your family's rise in politics has brought money and a circle of contacts. At eighteen, you sit across from people willing to help turn your experiments into a lab.")
                    : qsTr("The work you did at a borrowed desk has found its way into other people's conversations. At eighteen, you sit across from people willing to help you build a lab."),
                prompt: qsTr("What matters most in the work ahead?")
            }
        }
        for (const scene of scenes) {
            const copy = sceneCopy[scene.id] || {}
            add("chapter-" + scene.id, copy.art || workshop, copy.date || qsTr("CALIFORNIA  /  %1").arg(scene.date.slice(0, 4)),
                copy.title || scene.title, copy.narration || scene.text,
                copy.prompt || qsTr("What do you do?"), "chapter", { chapter: scene.id, choices: scene.choices })
            if (scene.id === "curiosity") {
                question("sociability", library, qsTr("CHILDHOOD  /  2004"), qsTr("A place at the table"),
                    qsTr("At the library, a group of children is pulling up chairs. There is also a quiet seat free beside the window."),
                    qsTr("Which seat do you take?"), [
                        option("outgoing", qsTr("I pull up a chair with the others")),
                        option("introverted", qsTr("I take the seat by the window"))
                    ])
                question("activity", homeArt, qsTr("CHILDHOOD  /  2005"), qsTr("The long way home"),
                    qsTr("School lets out. The walk home passes a park, with enough daylight left for a game."),
                    qsTr("What do you do with the time?"), [
                        option("athletic", qsTr("I stay and play until dusk")),
                        option("sedentary", qsTr("I head home to spend the afternoon indoors"))
                    ])
                question("disposition", library, qsTr("CHILDHOOD  /  2006"), qsTr("A broken computer"),
                    qsTr("The library computer freezes. The librarian is down the hall. You think you know what went wrong."),
                    qsTr("What is your first instinct?"), [
                        option("deliberate", qsTr("I work through what happened first")),
                        option("impulsive", qsTr("I try the fix that came to mind"))
                    ])
            }
        }

        add("return-home",homeArt,qsTr("SILICON VALLEY  /  JANUARY 2015"),qsTr("Back where it begins"),
            form.choices.arrival==="move"?qsTr("Your family followed opportunity away from the Valley. Now a new job brings them back, and your boxes return to a familiar California street."):qsTr("Silicon Valley has changed around your parents' home. You still know the sound of its front door."),
            "","narration",{afterword:qsTr("Yesterday was your eighteenth birthday. Tomorrow can wait. This morning is yours.")})
        const adult = qsTr("SILICON VALLEY  /  AGE 18")
        question("gender",homeArt,adult,qsTr("Your identity"),
            qsTr("You get to describe yourself. The notes show how people may respond in hiring and at work."),
            qsTr("What is your gender identity?"), [option("woman",qsTr("Woman"),story.genderAudience("woman")),
                option("man",qsTr("Man"),story.genderAudience("man")),option("nonbinary",qsTr("Nonbinary"),story.genderAudience("nonbinary")),
                option("unspecified",qsTr("Prefer not to say"),story.genderAudience("unspecified"))])
        question("pronouns",homeArt,adult,qsTr("The introduction"),qsTr("Choose the pronouns other people will use for you."),
            qsTr("Which pronouns fit?"), [option("she/her",qsTr("She / her")),option("he/him",qsTr("He / him")),
                option("they/them",qsTr("They / them")),option("other",qsTr("Other pronouns"),qsTr("Open additional pronoun sets."))])
        if(story.beat==="other_pronouns" || !["she/her","he/him","they/them"].includes(form.pronouns))
            question("other_pronouns",homeArt,adult,qsTr("In your own words"),qsTr("Choose the set to use in your profile and conversations."),
                qsTr("Which set fits?"),story.pronounOptions.filter(value=>!["she/her","he/him","they/them"].includes(value)).map(value=>option(value,value)))
        question("orientation",homeArt,adult,qsTr("Who you’re drawn to"),qsTr("At eighteen, you can decide what you want to share about yourself."),
            qsTr("How do you describe your sexual orientation?"), [option("straight",qsTr("Straight")),option("gay_or_lesbian",qsTr("Gay or lesbian")),
                option("bisexual",qsTr("Bisexual")),option("pansexual",qsTr("Pansexual")),option("asexual",qsTr("Asexual")),
                option("queer",qsTr("Queer")),option("questioning",qsTr("Questioning")),option("unspecified",qsTr("Prefer not to say"))])
        add("appearance",homeArt,qsTr("THE DAY AFTER YOUR EIGHTEENTH BIRTHDAY"),qsTr("Your reflection"),
            qsTr("Make this person your own. Your starting look is editable, from body shape to clothes and hair."),
            qsTr("Choose any feature to customize it."),"appearance")
        add("home",homeArt,qsTr("SILICON VALLEY  /  JANUARY 2015"),"","","","home")
        const reflections={
            roots:qsTr("Years later, neighbors still remember your family. Familiar streets become a network of people who know your name."),
            move:qsTr("Boxes replace the furniture. A new town means learning unfamiliar rules, and discovering that a fresh start can be both frightening and useful."),
            computer:qsTr("You stay until the librarian turns out the lights. The machine never gets bored of your questions; neither, for a while, do you."),
            library:qsTr("Next week, someone brings a friend. A table of strangers becomes a regular meeting, and you learn that enthusiasm can be contagious."),
            team:qsTr("You learn who needs encouragement, who wants space, and how much a team can depend on one quiet act of trust."),
            share:qsTr("A repaired tool passes from one household to another. You cannot fix the recession, but you can make the week easier for someone nearby."),
            scholarship:qsTr("Evenings disappear into applications and practice. A letter eventually arrives, opening a door your family could not have paid to unlock."),
            venture:qsTr("Customers remember a promise kept. You begin to notice the difference between an exciting idea and something people will actually buy."),
            science:qsTr("The project works after the third redesign. The best part is explaining why the first two failed, together, without anyone needing to hide it."),
            community:qsTr("People disagree about nearly every detail. You find enough common ground to get started, and a few become friends you can call years later."),
            business:qsTr("The till balances at the end of the day. Behind every small decision is a person trying to make rent or keep a customer."),
            reproduce:qsTr("The second run contradicts your favorite result. You fix the mistake before anyone sees it. The final claim is smaller, but it holds."),
            demonstrate:qsTr("You show the result and its limits. The room asks harder questions than you expected, and several people ask to see what comes next."),
            hype:qsTr("The headline travels farther than the caveats. Your name starts appearing in conversations you have never been part of."),
            publish:qsTr("A stranger reproduces the method and sends a useful correction. Something that was yours alone begins to belong to a wider conversation."),
            license:qsTr("The agreement takes three revisions. You leave with resources to continue and a clearer understanding of what you have promised."),
            private:qsTr("You close the workshop door and keep going. The work stays yours, though fewer people hear about it."),
            disclose:qsTr("You put the weak result on the screen. One reviewer nods and asks how they could help design the next test."),
            narrow:qsTr("You cross out the broad claim. The replacement says less, but you can defend every word."),
            sell:qsTr("The strongest chart fills the screen. The audience remembers it; the unanswered question remains in your notebook."),
            research:qsTr("You imagine a place where asking a difficult question is part of the job, and changing your mind is allowed."),
            people:qsTr("You think about the person on the other side of the screen. Their needs are less tidy than a benchmark, and more interesting."),
            independence:qsTr("You leave a blank page at the front of the notebook. Some decisions should stay open until you know more.")
        }
        const expanded=[]
        for(const beat of beats) {
            expanded.push(beat)
            if(["choice","chapter"].includes(beat.kind) && beat.id!=="birth_sex") {
                const selected=beat.kind==="chapter"?form.choices[beat.chapter]:beat.id==="other_pronouns"?form.pronouns:form[beat.id]
                const choice=(beat.choices || []).find(row=>row.id===selected)
                if(choice) expanded.push({id:beat.id+"-after",art:beat.art,tone:beat.tone,date:beat.date,title:choice.name,
                    narration:beat.id==="gender"?qsTr("You write it in your own words and tuck the old keepsake back into the family album."):choice.detail || qsTr("The choice becomes part of how you tell your story."),
                    prompt:"",kind:"narration",afterword:beat.id==="gender" && story.communitySupport(selected)?story.communitySupport(selected):reflections[selected] || qsTr("The years ahead will give you more chances to discover what this choice means to you.")})
            }
        }
        return expanded
    }
}
