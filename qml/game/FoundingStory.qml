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
            genius: qsTr("Learn Programming while growing up and you can take on the lab's first experiment yourself."),
            charisma: qsTr("You can talk your way into rooms that would otherwise stay closed."),
            political: form.family === "rich"
                ? qsTr("Your family is already wealthy. A parent enters politics, bringing public scrutiny and connections of your own.")
                : qsTr("A parent builds a political career. By the time you turn eighteen, your family has money, connections and a life in the public eye.")
        }
        question("leg", homeArt, qsTr("GROWING UP"), qsTr("An early advantage"),
            qsTr("Years from now, people will talk about how you got your start. Some of that story begins here."),
            qsTr("What will give you a head start?"), story.legs.filter(row => row.id !== "wealthy").map(row =>
                Object.assign({}, row, { detail: advantages[row.id] || row.detail })))

        const scenes = Array.from(story.chapters)
        const sceneCopy = {
            curiosity: { art: library, prompt: qsTr("Where do you spend your afternoons?") },
            setback: { art: homeArt, prompt: qsTr("Where do you put your energy?") },
            circle: { art: library, prompt: qsTr("Whose work do you join?") },
            promise: { art: workshop, prompt: qsTr("How do you show your work?") },
            ownership: { art: workshop, prompt: qsTr("What will you share?") },
            record: { art: workshop, prompt: qsTr("How do you answer the panel?") }
        }
        const activities = {
            robot_club: [workshop, qsTr("At the school activity fair, a classmate invites you to the robotics table to see what the team has built.")],
            theatre_club: [workshop, qsTr("At the school activity fair, a teacher asks you to help with the play and walks you to the rehearsal room.")],
            music_club: [workshop, qsTr("A classmate invites you to try the instruments in the school music room after class.")],
            skate_club: [street, qsTr("Friends from the park ask you to come with them to the skate park after school.")],
            garden_club: [street, qsTr("A neighbor from the park takes you to the school garden and introduces you to the teacher in charge.")],
            school_paper: [library, qsTr("A friend at the library asks you to come to a meeting of the school paper.")],
            sports_club: [street, qsTr("Someone you know from the park tells the after-school coach you've been playing with them. The coach invites you to practice.")],
            art_club: [workshop, form.activity === "athletic"
                ? qsTr("The next school year, a friend from the park asks you to help make a poster for their team. They take you to the art room and introduce you to the teacher.")
                : qsTr("The next school year, a classmate sees a sketch you made at home. They invite you to the art room after class and introduce you to the teacher.")],
            code_camp: [library, qsTr("After the school year ends, a friend from your group brings you a flyer for a local coding camp. You sign up together.")],
            maker_camp: [workshop, qsTr("Your group has been looking for a place to build over the summer. A mentor at the local makerspace offers you a bench.")],
            network_camp: [library, qsTr("The librarian remembers your visits and tells you about a summer computer workshop. On the first morning, you meet the other campers in the library.")],
            film_camp: [workshop, qsTr("A friend from your group asks you to join a summer film camp. You meet the crew on the first morning.")],
            civic_camp: [library, qsTr("One of the people you work with finds a youth civic workshop. You sign up and head to its first meeting together.")],
            volunteer_camp: [street, qsTr("Someone from your neighborhood group introduces you to the staff at a community center. You volunteer there during the summer.")],
            music_camp: [workshop, qsTr("As summer begins, a friend invites you to join their ensemble at music camp.")],
            sports_camp: [street, qsTr("The local coach invites you to a summer camp. You return to the courts and meet the new players.")],
        }
        for (const scene of scenes) {
            const copy = sceneCopy[scene.id] || {}
            const activity = activities[scene.id]
            const earlier = scene.requiresChoice ? scenes.find(row => row.id === scene.requiresChoice.split("/")[0]) : null
            const earlierActivity = earlier ? activities[earlier.id] : null
            const project = {
                science: qsTr("You keep working with friends from the science fair. "),
                community: qsTr("Your neighborhood group needs a tool to organize its work. You volunteer to build it. "),
                business: qsTr("The shopkeeper tells you about a job they do by hand every week. You build a small program to help. ")
            }[form.choices.circle]
            const narration = activity ? activity[1] + " " + scene.text
                : earlierActivity ? qsTr("After weeks with the same group, it’s time to put your work in front of other people. ") + scene.text
                : scene.id === "promise" && project ? project + scene.text : scene.text
            add("chapter-" + scene.id, copy.art || (activity || earlierActivity || [workshop])[0], qsTr("CALIFORNIA  /  %1").arg(scene.date.slice(0, 4)),
                scene.title, narration,
                copy.prompt || qsTr("What do you do?"), "chapter", { chapter: scene.id, choices: scene.choices })
            if (scene.id === "curiosity") {
                question("sociability", library, qsTr("CHILDHOOD  /  2004"), qsTr("A place at the table"),
                    form.choices.curiosity === "team"
                        ? qsTr("The next school year, a teammate invites you to do homework at the library before practice. You can join the group or work at a quiet desk.")
                        : qsTr("The next school year, the librarian sets out chairs for an after-school homework group. There’s also a quiet seat by the window."),
                    qsTr("Which seat do you take?"), [
                        option("outgoing", qsTr("I pull up a chair with the others")),
                        option("introverted", qsTr("I take the seat by the window"))
                    ])
                question("activity", homeArt, qsTr("CHILDHOOD  /  2005"), qsTr("The long way home"),
                    form.sociability === "outgoing"
                        ? qsTr("The following spring, some of the kids from the library invite you to play in the park on your way home.")
                        : qsTr("The following spring, you walk home from school on your own. A group at the park calls over to ask if you want to play."),
                    qsTr("What do you do with the time?"), [
                        option("athletic", qsTr("I stay and play until dusk")),
                        option("sedentary", qsTr("I head home to spend the afternoon indoors"))
                    ])
                question("disposition", library, qsTr("CHILDHOOD  /  2006"), qsTr("A broken computer"),
                    form.activity === "athletic"
                        ? qsTr("The next year, rain cancels an afternoon game. You go to the library to finish a school project. The computer freezes before you can save.")
                        : qsTr("The next year, you need a library computer to finish a school project. The screen freezes before you can save."),
                    qsTr("What is your first instinct?"), [
                        option("deliberate", qsTr("I ask the librarian how to recover my work")),
                        option("impulsive", qsTr("I restart the computer right away"))
                    ])
            }
        }

        add("eighteen",homeArt,qsTr("SILICON VALLEY  /  JANUARY 2015"),qsTr("A morning of your own"),
            qsTr("The Valley has changed around the home you grew up in. Yesterday was your eighteenth birthday. Birthday cards still stand on your desk."),
            "","narration",{afterword:qsTr("There is no company yet, no mission to promise. First, there is the person you have become.")})
        const adult = qsTr("SILICON VALLEY  /  AGE 18")
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
        add("home",homeArt,qsTr("SILICON VALLEY  /  JANUARY 2015"),"","","","home")
        const reflections={
            athletic: qsTr("You head home as the park lights come on. You make plans to meet again after school."),
            sedentary: qsTr("You spend the afternoon at home with enough quiet to get absorbed in a project."),
            deliberate: qsTr("The librarian shows you where the program keeps its recovery copies. Most of your work is there."),
            impulsive: qsTr("The computer starts again, but the last few lines are gone. You remember to save early the next time."),
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
            if(beat.id === "other_pronouns" && story.beat !== "other_pronouns" && ["she/her","he/him","they/them"].includes(form.pronouns)) continue
            expanded.push(beat)
            if(["choice","chapter"].includes(beat.kind)) {
                const selected=beat.kind==="chapter"?form.choices[beat.chapter]:beat.id==="other_pronouns"?form.pronouns:form[beat.id]
                const choice=(beat.choices || []).find(row=>row.id===selected)
                if(choice && (choice.detail || reflections[selected] || ["gender","orientation","other_pronouns"].includes(beat.id))) expanded.push({id:beat.id+"-after",art:beat.art,tone:beat.tone,date:beat.date,title:choice.name,
                    narration:beat.id==="gender"?qsTr("You write it in your own words and tuck the old keepsake back into the family album."):choice.detail || "",
                    prompt:"",kind:"narration",afterword:["gender","orientation"].includes(beat.id) && story.communitySupport(form.gender)?story.communitySupport(form.gender):reflections[selected] || (beat.id === "other_pronouns" ? qsTr("These are the pronouns people will use for you.") : "")})
            }
        }
        return expanded
    }
}
