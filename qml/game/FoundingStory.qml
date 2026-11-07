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
        field("first_name", hospital, birth, qsTr("The name card"),
            qsTr("The nurse slips a blank card into the holder at the foot of the bassinet. Your family has a name in mind."),
            qsTr("What first name do they give you?"), 63)
        field("last_name", hospital, birth, qsTr("A name to carry"),
            qsTr("She writes it down, leaving room on the card for the name you will carry out of the ward."),
            qsTr("What is your surname?"), 63)
        question("family", street, qsTr("HOME  /  1997"), qsTr("The house you come home to"),
            qsTr("By morning, the rain has passed. Your parents take you home, to the street where your earliest memories will begin."),
            qsTr("How much did your family have?"), story.families.map(row => ({
                id: row.id,
                name: row.id === "poor" ? qsTr("We counted every dollar") :
                    row.id === "middle" ? qsTr("We had enough to get by") :
                    row.id === "rich" ? qsTr("We had money to spare") : row.name
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
            if (scene.id === "record") {
                const date = qsTr("YOUR FIRST PUBLIC TEST  /  2014")
                field("gender", workshop, date, qsTr("In your own words"),
                    qsTr("Ahead of your first public test, an organizer sends over a short biography for you to check."),
                    qsTr("How do you describe your gender?"), 64)
                field("pronouns", workshop, date, qsTr("The introduction"),
                    qsTr("You hand the page back. The organizer will read it aloud before you take your seat."),
                    qsTr("Which pronouns should they use?"), 64)
            }
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

        add("threshold", workshop, founding, qsTr("The door is open"),
            qsTr("You turn eighteen. On the table are a key, a stack of papers, and a notebook full of experiments you want to run."),
            "", "narration", { afterword: qsTr("For the first time, the decision is yours to sign.") })
        question("legal_form", workshop, founding, qsTr("The charter"),
            qsTr("The first page names the institution you are about to create. It sets the rules for who can fund it and who holds the rights."),
            qsTr("What kind of institution do you found?"), [
                option("corporation", qsTr("For-profit corporation"), qsTr("Issue equity to raise capital and share profits with owners.")),
                option("nonprofit", qsTr("Nonprofit research institution"), qsTr("Fund the mission with gifts and answer to trustees. The institution owns its assets."))
            ])
        question("route", foundingArt, founding, qsTr("A way in"),
            qsTr("The offers on the table reflect what you have done and who is willing to take a chance on you."),
            qsTr("How do you get the lab off the ground?"), story.routes.filter(row => row.available))
        field("company", foundingArt, founding, qsTr("A name on the door"),
            qsTr("Your signature is ready. The line for the institution is still blank."),
            qsTr("What will you call it?"), 128)
        question("mission", foundingArt, founding, qsTr("The reason for starting"),
            qsTr("People will join, leave, and disagree about what this place is for. The first statement of purpose is yours to write."),
            qsTr("What are you building toward?"), [
                option("survival", qsTr("Build a lab that lasts"), qsTr("Stay solvent and active through 2115.")),
                option("utopia", qsTr("Make lives better"), qsTr("Sustain welfare in the last decade.")),
                option("monopoly", qsTr("Build the leading AI platform"), qsTr("Lead useful work over the last five years.")),
                option("domination", qsTr("Steer the AI age"), qsTr("Earn durable industry authority.")),
                option("asi", qsTr("Demonstrate ASI first"), qsTr("Beat rivals to independent validation.")),
                { id: "profit", name: qsTr("Leave a financial legacy"), detail: qsTr("Build the most founder wealth by 2115."),
                    available: form.legal_form !== "nonprofit", reason: qsTr("Requires a company with founder equity.") }
            ])
        const offers = {
            frontier: qsTr("Kestrel's partner opens your notebook to the most ambitious experiment. That's the work they want to fund."),
            acceleration: qsTr("Morrow wants your research in people's hands quickly. Its partner asks how soon you can open it up."),
            commercial: qsTr("Harbor's partner asks to meet the first customer. They want evidence of demand and a plan to cover the costs."),
            compute: qsTr("Copperline offers capital and a connection to a compute supplier. You would be tying the two relationships together."),
            safety: qsTr("The foundation offers to fund the research. Its trustee wants independent evaluations and a public record of the results."),
            open_science: qsTr("The trust wants others to be able to reproduce the work and use what they learn. It offers to help pay for that.")
        }
        for (const backer of story.backers.filter(row => row.available)) {
            const narration = offers[backer.id] ? offers[backer.id] + " " + backer.terms : backer.detail
            add("offer-" + backer.id, foundingArt, founding, backer.name, narration,
                qsTr("Do you accept the offer?"), "offer", {
                    offer: backer.id,
                    choices: [
                        option("accept", qsTr("Accept the offer")),
                        option("decline", qsTr("Decline the offer"))
                    ]
                })
        }
        question("headquarters", foundingArt, founding, qsTr("An address"),
            qsTr("The registration papers ask for the state where the lab will do its work."),
            qsTr("Where will you put down roots?"), story.headquarters)
        question("office", foundingArt, founding, qsTr("A room to work in"),
            qsTr("Every month in a room you can afford buys another month of experiments."),
            qsTr("Where will you set up your first desks?"), story.offices)
        question("researcher", foundingArt, founding, qsTr("The first experiment"),
            qsTr("The first notebook is open on the desk. Someone needs to do the work inside it."),
            qsTr("Who takes on the research?"), story.researchers)
        const difficultyNotes = {
            "difficulty.forgiving": qsTr("More starting cash and slower rival decisions."),
            "difficulty.standard": qsTr("Standard funding and rival pace."),
            "difficulty.demanding": qsTr("Tighter funding and faster rival decisions.")
        }
        question("difficulty", foundingArt, founding, qsTr("The road ahead"),
            qsTr("The lab is nearly ready. Take a moment to choose the conditions under which it will begin."),
            qsTr("How demanding should the campaign be?"), story.difficulties.map(row =>
                Object.assign({}, row, { detail: difficultyNotes[row.id] || row.detail })))

        add("review", foundingArt, founding, qsTr("The first morning"),
            qsTr("%1 has a name, a place to work, and its first experiment ahead. Beyond the door, a new year is beginning.").arg(form.company),
            "", "review")
        field("seed", foundingArt, founding, qsTr("World seed"), "",
            qsTr("Which seed should this campaign use?"), 20)
        return beats
    }
}
