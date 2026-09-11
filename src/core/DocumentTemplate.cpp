// DocumentTemplate.cpp

#include "DocumentTemplate.h"

#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <initializer_list>

namespace {
DocumentSection section(const QString& id, int level, int order, const QString& sv, const QString& en,
                        const QString& guidanceSv, const QString& guidanceEn, bool required,
                        const QString& parent = {})
{
    return {id, level, order, sv, en, guidanceSv, guidanceEn, required, parent};
}

DocumentSection major(const QString& id, int order, const QString& sv, const QString& en,
                      const QString& guidanceSv, const QString& guidanceEn, bool required = true)
{
    return section(id, 1, order, sv, en, guidanceSv, guidanceEn, required);
}

DocumentSection minor(const QString& parent, const QString& id, int order, const QString& sv, const QString& en,
                      const QString& guidanceSv, const QString& guidanceEn, bool required = true)
{
    return section(id, 2, order, sv, en, guidanceSv, guidanceEn, required, parent);
}

QJsonObject sectionJson(const DocumentSection& section)
{
    return {{QStringLiteral("id"), section.id}, {QStringLiteral("level"), section.level},
            {QStringLiteral("order"), section.order}, {QStringLiteral("title"), QJsonObject{{QStringLiteral("sv"), section.titleSv}, {QStringLiteral("en"), section.titleEn}}},
            {QStringLiteral("guidance"), QJsonObject{{QStringLiteral("sv"), section.guidanceSv}, {QStringLiteral("en"), section.guidanceEn}}},
            {QStringLiteral("required"), section.required}, {QStringLiteral("parentId"), section.parentId}};
}

DocumentTemplate make(const QString& id, const QString& type, std::initializer_list<DocumentSection> sections)
{
    DocumentTemplate result{id, type, 1, QList<DocumentSection>(sections)};
    return result;
}
}

namespace DocumentTemplates {

DocumentTemplate thesis()
{
    return make(QStringLiteral("aramf-default-thesis"), QStringLiteral("thesis"), {
        major("title-page", 1, "Titelsida", "Title Page", "Ange titel, författare, utbildning, institution och datum enligt lärosätets krav. Undvik att lägga resultat eller långa beskrivningar på titelsidan.", "State the title, author, programme, institution, and date according to the institution's requirements. Do not place results or long explanations on the title page.", true),
        major("abstract", 2, "Sammanfattning", "Abstract", "Sammanfatta problem, syfte, metod och de viktigaste resultaten kort. Texten ska kunna förstås utan resten av rapporten och ska inte innehålla nya påståenden.", "Summarize the problem, purpose, method, and most important results briefly. The text should stand on its own and introduce no claims that are absent from the thesis.", true),
        major("preface", 3, "Förord", "Preface", "Tacka personer eller organisationer och nämn praktiska omständigheter som hjälpt arbetet. Håll avsnittet personligt men undvik saklig argumentation som hör hemma i huvudtexten.", "Acknowledge people or organisations and mention practical circumstances that supported the work. Keep it personal and move substantive argumentation to the main text.", false),
        major("table-of-contents", 4, "Innehållsförteckning", "Table of Contents", "Denna innehållsförteckning ska skapas från dokumentets kanoniska rubrikhierarki. Redigera inte en separat manuell lista.", "Generate this table of contents from the canonical heading hierarchy. Do not maintain a separate manually edited list.", true),
        major("list-of-figures", 5, "Figurförteckning", "List of Figures", "Ta med en figurförteckning när dokumentet innehåller flera figurer. Varje figur ska ha begriplig bildtext och hänvisas till i brödtexten.", "Include a list of figures when the document contains several figures. Every figure needs a meaningful caption and a reference in the body text.", false),
        major("list-of-tables", 6, "Tabellförteckning", "List of Tables", "Ta med en tabellförteckning när tabeller används återkommande. Kontrollera att tabellerna tillför information och inte bara upprepar text.", "Include a list of tables when tables are used repeatedly. Ensure that tables add information instead of merely repeating the prose.", false),
        major("abbreviations-glossary", 7, "Förkortningar och begrepp", "Abbreviations and Glossary", "Förklara förkortningar och centrala specialbegrepp som läsaren behöver. Definiera inte sådant som redan är allmänt känt för målgruppen.", "Explain abbreviations and essential specialist terms. Do not define ordinary terms that are already clear to the intended audience.", false),
        major("introduction", 8, "Inledning", "Introduction", "Introducera området, problemet, arbetets syfte och hur dokumentet är uppbyggt. Inledningen ska ge läsaren en tydlig väg in i arbetet utan att föregripa hela diskussionen.", "Introduce the field, problem, purpose, and structure of the document. The introduction should orient the reader without pre-empting the entire discussion.", true),
        minor("introduction", "introduction-background", 9, "Bakgrund", "Background", "Beskriv den bakgrund som behövs för att förstå problemet och varför det är relevant. Avgränsa historiken till sådant som stödjer arbetets fokus.", "Describe the background needed to understand the problem and its relevance. Limit history to information that supports the focus of the work."),
        minor("introduction", "introduction-problem", 10, "Problembeskrivning", "Problem Description", "Formulera vilket problem, behov eller kunskapslucka arbetet behandlar. Skilj problemet från den lösning eller metod som väljs senare.", "Define the problem, need, or knowledge gap addressed by the work. Keep the problem distinct from the solution or method selected later."),
        minor("introduction", "introduction-purpose", 11, "Syfte", "Purpose", "Beskriv varför arbetet genomförs och vilket övergripande problem eller behov arbetet avser att behandla. Syftet ska vara tydligt, avgränsat och kopplat till problembeskrivningen.", "Describe why the work is carried out and the overall problem or need it addresses. The purpose should be clear, delimited, and connected to the problem description."),
        minor("introduction", "introduction-research-questions", 12, "Frågeställningar", "Research Questions", "Formulera frågor som kan besvaras med arbetets metod och resultat. Undvik frågor som är för breda, otestbara eller inte följs upp senare.", "Formulate questions that can be answered by the method and results. Avoid questions that are too broad, untestable, or not addressed later."),
        minor("introduction", "introduction-objectives", 13, "Mål", "Objectives", "Bryt ned syftet i konkreta mål eller leverabler. Visa senare i resultat och slutsats hur målen uppnåddes eller varför de inte kunde nås.", "Break the purpose into concrete objectives or deliverables. Later, show in the results and conclusion how the objectives were met or why they could not be met."),
        minor("introduction", "introduction-scope", 14, "Avgränsningar", "Scope and Limitations", "Förklara vad arbetet omfattar och inte omfattar samt varför. Avgränsningar ska göra arbetet genomförbart och återkomma i tolkningen av resultaten.", "Explain what the work does and does not cover and why. Scope decisions make the work feasible and should be considered when interpreting results."),
        minor("introduction", "introduction-structure", 15, "Rapportens struktur", "Document Structure", "Ge en kort vägledning genom kapitlen och deras funktion. Upprepa inte hela innehållsförteckningen eller presentera resultat här.", "Briefly guide the reader through the chapters and their purpose. Do not reproduce the entire table of contents or present results here.", false),
        major("theory-background", 16, "Teori och bakgrund", "Theory and Background", "Bygg den teoretiska ram som behövs för att förstå och bedöma arbetet. Koppla teori och tidigare arbete till frågeställningarna i stället för att samla fristående fakta.", "Build the theoretical framework needed to understand and assess the work. Connect theory and prior work to the research questions rather than collecting disconnected facts.", true),
        minor("theory-background", "theory-foundation", 17, "Teoretisk grund", "Theoretical Foundation", "Presentera teorier och modeller som används för att resonera om problemet. Förklara centrala antaganden och hänvisa till tillförlitliga källor.", "Present theories and models used to reason about the problem. Explain key assumptions and cite reliable sources."),
        minor("theory-background", "theory-concepts", 18, "Relevanta begrepp", "Relevant Concepts", "Definiera begrepp så att de används konsekvent genom dokumentet. Undvik långa uppslagsverksdefinitioner utan koppling till arbetet.", "Define terms so they are used consistently throughout the document. Avoid lengthy dictionary definitions without a connection to the work."),
        minor("theory-background", "theory-technologies-methods-models", 19, "Tekniker, metoder eller modeller", "Technologies, Methods or Models", "Förklara teknik, metodik eller modeller som läsaren behöver för att förstå genomförandet. Lägg projektspecifika val och resultat i senare kapitel när det passar bättre.", "Explain technologies, methods, or models needed to understand the work. Put project-specific choices and results in later chapters where appropriate."),
        minor("theory-background", "theory-related-work", 20, "Relaterat arbete", "Related Work", "Jämför relevant tidigare arbete och visa vilken relation det har till den aktuella frågan. Undvik att bara rada upp källor utan analys.", "Compare relevant prior work and explain its relation to the current question. Do not merely list sources without analysis."),
        minor("theory-background", "theory-summary", 21, "Sammanfattning av teorigrund", "Summary of Theoretical Framework", "Sammanfatta vilka delar av teorin som blir viktiga i metod, resultat och diskussion. Använd sammanfattningen som en brygga till metodkapitlet.", "Summarize which parts of the theory matter for the method, results, and discussion. Use the summary as a bridge to the method chapter.", false),
        major("method", 22, "Metod", "Method", "Beskriv hur arbetet genomfördes så att läsaren kan bedöma tillvägagångssättet och i rimlig grad upprepa det. Redovisa inte resultat som hör hemma i resultatkapitlet.", "Describe how the work was conducted so the reader can assess and, where reasonable, reproduce the approach. Do not report results that belong in the results chapter.", true),
        minor("method", "method-selection", 23, "Metodval", "Method Selection", "Beskriv vilken metod eller kombination av metoder som valts och motivera varför de passar syfte och frågeställningar. Nämn relevanta alternativ när det klargör valet.", "Describe the method or combination selected and justify why it suits the purpose and research questions. Discuss relevant alternatives where that clarifies the choice."),
        minor("method", "method-work-process", 24, "Arbetsprocess", "Work Process", "Redovisa arbetets viktigaste steg i en logisk ordning, inklusive iterationer när de påverkar resultatet. Undvik en dagbok med oviktiga detaljer.", "Present the important steps in a logical order, including iterations that affect the outcome. Avoid a diary of insignificant details."),
        minor("method", "method-development-investigation", 25, "Utvecklings- eller undersökningsmetod", "Development or Investigation Method", "Förklara hur lösningen utvecklades eller hur undersökningen genomfördes, inklusive urval, mätningar eller analys. Knyt varje steg till frågeställningarna.", "Explain how the solution was developed or the investigation conducted, including sampling, measurements, or analysis. Tie each step to the research questions."),
        minor("method", "method-tools-environment", 26, "Verktyg och miljö", "Tools and Environment", "Dokumentera verktyg, versioner, miljöer och andra förutsättningar som kan påverka arbetets resultat. Undvik produktreklam och irrelevanta specifikationer.", "Document tools, versions, environments, and other conditions that may affect the results. Avoid product promotion and irrelevant specifications."),
        minor("method", "method-data-material-sources", 27, "Data, material eller källor", "Data, Material or Sources", "Beskriv vilket material som användes, hur det valdes och hur dess kvalitet hanterades. Ange ursprung och begränsningar utan att duplicera referenslistan.", "Describe the material used, how it was selected, and how its quality was handled. State origin and limitations without duplicating the reference list."),
        minor("method", "method-testing-validation", 28, "Test- och valideringsmetod", "Testing and Validation Method", "Förklara vad som testades eller validerades, mot vilka kriterier och varför. Skilj testupplägg från de faktiska testresultaten.", "Explain what was tested or validated, against which criteria, and why. Keep the test design separate from the actual test results."),
        minor("method", "method-reliability-validity", 29, "Reliabilitet och validitet", "Reliability and Validity", "Diskutera hur tillförlitliga och giltiga metoder, mätningar och slutsatser kan vara. Var konkret om felkällor och deras möjliga påverkan.", "Discuss the reliability and validity of methods, measurements, and conclusions. Be concrete about sources of error and their possible effects."),
        minor("method", "method-ethics", 30, "Etiska överväganden", "Ethical Considerations", "Redovisa etiska frågor, integritet, säkerhet, miljö eller ansvar som är relevanta för arbetet och hur de hanterades. Utelämna inte relevanta risker.", "Report relevant ethical, privacy, safety, environmental, or responsibility issues and how they were handled. Do not omit material risks.", false),
        major("implementation", 31, "Genomförande", "Implementation", "Beskriv hur idéer, krav och metod omsattes i det faktiska arbetet. Fokusera på beslut och lösningar som behövs för att förstå resultaten, inte på varje detalj.", "Describe how ideas, requirements, and method were turned into the actual work. Focus on decisions and solutions needed to understand the results, not every detail.", true),
        minor("implementation", "implementation-overview", 32, "Översikt", "Overview", "Ge en sammanhängande översikt över genomförandet och hur delarna hänger ihop. Använd figurer eller tabeller när de gör strukturen tydligare.", "Give a coherent overview of the implementation and how its parts fit together. Use figures or tables when they clarify the structure."),
        minor("implementation", "implementation-requirements", 33, "Krav", "Requirements", "Redovisa relevanta funktionella och icke-funktionella krav samt hur de härleddes. Markera vilka krav som faktiskt utvärderas senare.", "Present relevant functional and non-functional requirements and how they were derived. Identify which requirements are evaluated later."),
        minor("implementation", "implementation-architecture-design", 34, "Arkitektur eller design", "Architecture or Design", "Förklara den övergripande strukturen och viktiga gränssnitt eller principer. Motivera val som påverkar kvalitet, användning eller vidareutveckling.", "Explain the overall structure and important interfaces or principles. Justify choices affecting quality, use, or future development."),
        minor("implementation", "implementation-details", 35, "Implementation", "Implementation", "Beskriv centrala delar av lösningen på en nivå som stödjer förståelsen av resultaten. Undvik att klistra in stora kodmängder eller irrelevanta detaljer.", "Describe central parts of the solution at a level that supports understanding of the results. Avoid large code listings and irrelevant details."),
        minor("implementation", "implementation-decisions", 36, "Viktiga designbeslut", "Important Design Decisions", "Redovisa beslut, alternativ och konsekvenser när de har betydelse för resultatet. Låt beslutens motivering vara spårbar till krav eller teori.", "Report decisions, alternatives, and consequences when they matter to the results. Make the rationale traceable to requirements or theory."),
        minor("implementation", "implementation-testing", 37, "Testning och verifiering", "Testing and Verification", "Beskriv hur lösningen verifierades i praktiken och koppla verifieringen till kraven. Spara mätvärden och utfall till resultatkapitlet.", "Describe how the solution was verified in practice and connect verification to requirements. Put measurements and outcomes in the results chapter."),
        minor("implementation", "implementation-deviations", 38, "Avvikelser och förändringar", "Deviations and Changes", "Förklara viktiga avvikelser från planen eller ursprungliga krav och deras konsekvenser. Dölj inte förändringar som påverkar tolkningen.", "Explain important deviations from the plan or original requirements and their consequences. Do not hide changes that affect interpretation.", false),
        major("results", 39, "Resultat", "Results", "Presentera arbetets resultat sakligt och strukturerat: vad som framkommit, implementerats, mätts eller verifierats. Djupare tolkning hör normalt hemma i diskussionen.", "Present the results objectively and in a structured manner: what was found, implemented, measured, or verified. Deeper interpretation normally belongs in the discussion.", true),
        minor("results", "results-main", 40, "Huvudresultat", "Main Results", "Redovisa de resultat som direkt besvarar syfte och frågeställningar. Ange tillräckligt underlag för att läsaren ska kunna följa resonemanget.", "Present results that directly address the purpose and research questions. Provide enough evidence for the reader to follow the reasoning."),
        minor("results", "results-tests", 41, "Testresultat", "Test Results", "Redovisa testdata, utfall och relevanta kriterier utan att överdriva säkerheten. Skilj observationer från tolkningar.", "Report test data, outcomes, and relevant criteria without overstating certainty. Keep observations separate from interpretations."),
        minor("results", "results-observations", 42, "Observationer", "Observations", "Beskriv mönster eller iakttagelser som är viktiga för förståelsen men inte passar som huvudresultat. Ange när en observation är osäker.", "Describe patterns or observations important to understanding but not suitable as main results. State when an observation is uncertain."),
        minor("results", "results-objectives", 43, "Jämförelse med mål", "Comparison with Objectives", "Visa vilka mål som uppnåddes och vilket underlag som stöder bedömningen. Förklara neutralt mål som inte nåddes.", "Show which objectives were met and what evidence supports the assessment. Explain unmet objectives neutrally."),
        major("discussion", 44, "Diskussion", "Discussion", "Tolka resultaten i relation till syfte, mål, teori och vald metod. Diskutera styrkor, svagheter, osäkerheter och relevanta begränsningar.", "Interpret the results in relation to purpose, objectives, theory, and method. Discuss strengths, weaknesses, uncertainties, and relevant limitations.", true),
        minor("discussion", "discussion-results", 45, "Resultatdiskussion", "Result Discussion", "Förklara vad resultaten betyder och varför de kan ha blivit som de blev. Skilj välgrundade slutsatser från spekulation.", "Explain what the results mean and why they may have occurred. Distinguish well-supported conclusions from speculation."),
        minor("discussion", "discussion-method", 46, "Metoddiskussion", "Method Discussion", "Bedöm metodens lämplighet och hur andra metodval kunde ha påverkat resultatet. Upprepa inte bara metodkapitlet.", "Assess the suitability of the method and how alternatives might have affected the results. Do not merely repeat the method chapter."),
        minor("discussion", "discussion-reliability", 47, "Tillförlitlighet och begränsningar", "Reliability and Limitations", "Samla de viktigaste osäkerheterna och begränsningarna och förklara deras praktiska betydelse. Knyt dem till avgränsningar och validitet.", "Bring together the most important uncertainties and limitations and explain their practical significance. Connect them to scope and validity."),
        minor("discussion", "discussion-related-work", 48, "Jämförelse med relaterat arbete", "Comparison with Related Work", "Jämför resultaten med tidigare arbete och förklara möjliga likheter och skillnader. Var försiktig när studierna inte är direkt jämförbara.", "Compare the results with prior work and explain possible similarities and differences. Be cautious when studies are not directly comparable."),
        minor("discussion", "discussion-lessons", 49, "Lärdomar", "Lessons Learned", "Sammanfatta lärdomar som kan hjälpa framtida arbete eller liknande projekt. Fokusera på generaliserbara insikter, inte personliga detaljer.", "Summarize lessons that can help future work or similar projects. Focus on transferable insights rather than personal details."),
        major("conclusion", 50, "Slutsats", "Conclusion", "Avsluta genom att besvara vad arbetet visar i relation till syfte och frågeställningar. Introducera inte nya resultat eller nya källor här.", "Conclude by stating what the work shows in relation to its purpose and questions. Do not introduce new results or sources here.", true),
        minor("conclusion", "conclusion-findings", 51, "Slutsatser", "Conclusions", "Formulera tydliga slutsatser som kan härledas från resultat och diskussion. Undvik bredare påståenden än underlaget tillåter.", "State clear conclusions that follow from the results and discussion. Avoid claims broader than the evidence permits."),
        minor("conclusion", "conclusion-questions", 52, "Svar på frågeställningar", "Answers to Research Questions", "Besvara varje frågeställning uttryckligen och hänvisa till relevant underlag. Lämna inte läsaren att själv gissa kopplingen.", "Answer each research question explicitly and refer to the relevant evidence. Do not make the reader infer the connection."),
        minor("conclusion", "conclusion-future", 53, "Framtida arbete", "Future Work", "Föreslå realistiska nästa steg som följer av resultaten och begränsningarna. Motivera varför de är värdefulla.", "Suggest realistic next steps that follow from the results and limitations. Explain why they would be valuable."),
        major("references", 54, "Referenser", "References", "Lista endast källor som faktiskt används och hänvisa konsekvent till dem i texten. Följ vald referensstil och kontrollera fullständighet.", "List only sources actually used and cite them consistently in the text. Follow the selected citation style and check completeness.", true),
        major("appendices", 55, "Bilagor", "Appendices", "Lägg kompletterande material som stöder arbetet men skulle störa huvudflödet här. Hänvisa till varje relevant bilaga från huvudtexten.", "Place supplementary material that supports the work but would interrupt the main flow here. Refer to each relevant appendix from the body text.", false)
    });
}

DocumentTemplate report()
{
    return make(QStringLiteral("aramf-default-report"), QStringLiteral("report"), {
        major("title-page", 1, "Titelsida", "Title Page", "Ange titel, författare, organisation eller utbildning och datum enligt dokumentets sammanhang. Håll sidan tydlig och fri från resultat.", "State the title, author, organisation or programme, and date appropriate to the context. Keep the page clear and free of results.", true),
        major("summary", 2, "Sammanfattning", "Summary", "Sammanfatta dokumentets syfte, genomförande och viktigaste resultat kort. Ta inte med påståenden som inte utvecklas i rapporten.", "Summarize the report's purpose, execution, and key results briefly. Do not make claims that are not developed in the report.", true),
        major("table-of-contents", 3, "Innehållsförteckning", "Table of Contents", "Generera innehållsförteckningen från den kanoniska rubrikhierarkin. Underhåll inte en separat manuell lista.", "Generate the table of contents from the canonical heading hierarchy. Do not maintain a separate manual list.", true),
        major("introduction", 4, "Inledning", "Introduction", "Förklara ämnet, varför dokumentet behövs och hur resten av rapporten är organiserad. Spara detaljerade resultat till resultatkapitlet.", "Explain the subject, why the report is needed, and how the rest is organised. Save detailed results for the results chapter.", true),
        minor("introduction", "introduction-background", 5, "Bakgrund", "Background", "Ge den bakgrund som behövs för att förstå frågan och dess sammanhang. Undvik information som inte hjälper läsaren att följa rapporten.", "Provide the background needed to understand the question and context. Avoid information that does not help the reader follow the report."),
        minor("introduction", "introduction-purpose", 6, "Syfte", "Purpose", "Beskriv vad rapporten ska åstadkomma eller klargöra och koppla syftet till bakgrunden. Formulera det så att resultatet senare kan bedömas.", "Describe what the report aims to achieve or clarify and connect it to the background. Phrase it so the result can later be assessed."),
        minor("introduction", "introduction-objectives", 7, "Mål", "Objectives", "Ange konkreta mål eller leverabler som gör syftet mätbart eller granskningsbart. Visa senare hur de hanterades.", "State concrete objectives or deliverables that make the purpose assessable. Later show how they were addressed."),
        minor("introduction", "introduction-scope", 8, "Avgränsning", "Scope", "Beskriv vilka delar som ingår och inte ingår och varför. Tydliga avgränsningar hjälper läsaren att tolka rapportens anspråk.", "Describe what is and is not included and why. Clear scope helps the reader interpret the report's claims."),
        major("context", 9, "Bakgrund och förutsättningar", "Context", "Beskriv förutsättningar, krav och begrepp som behövs för att förstå arbetet. Håll kapitlet relevant för rapportens syfte.", "Describe the conditions, requirements, and concepts needed to understand the work. Keep the chapter relevant to the report's purpose.", true),
        minor("context", "context-setting", 10, "Kontext", "Context", "Förklara sammanhanget där arbetet utfördes eller ska användas. Skilj dokumenterade förutsättningar från egna antaganden.", "Explain the context in which the work was carried out or will be used. Distinguish documented conditions from your assumptions."),
        minor("context", "context-requirements", 11, "Krav", "Requirements", "Sammanfatta relevanta krav och hur de påverkar arbetets utformning eller bedömning. Undvik krav som inte används vidare.", "Summarize relevant requirements and how they affect design or assessment. Avoid requirements that are not used later."),
        minor("context", "context-concepts", 12, "Relevanta begrepp", "Relevant Concepts", "Definiera centrala begrepp på ett sätt som passar rapportens användning. Var konsekvent med orden genom hela dokumentet.", "Define key terms in a way suited to the report's use. Use the terms consistently throughout the document."),
        minor("context", "context-assumptions", 13, "Antaganden", "Assumptions", "Redovisa antaganden som påverkar genomförande eller resultat och ange vad som händer om de inte gäller. Dölj inte osäkerhet.", "State assumptions affecting execution or results and what happens if they do not hold. Do not hide uncertainty.", false),
        major("method", 14, "Metod", "Method", "Förklara hur arbetet planerades och genomfördes så att läsaren kan bedöma resultatens underlag. Separera tillvägagångssätt från utfall.", "Explain how the work was planned and conducted so the reader can assess the basis of the results. Keep approach separate from outcome.", true),
        minor("method", "method-approach", 15, "Angreppssätt", "Approach", "Beskriv och motivera det övergripande angreppssättet. Förklara varför det passar syfte och krav.", "Describe and justify the overall approach. Explain why it suits the purpose and requirements."),
        minor("method", "method-work-process", 16, "Arbetsprocess", "Work Process", "Redovisa de viktigaste arbetsstegen i en begriplig ordning och nämn förändringar som påverkar utfallet.", "Present the important work steps in a clear order and mention changes that affect the outcome."),
        minor("method", "method-tools", 17, "Verktyg och metoder", "Tools and Methods", "Dokumentera relevanta verktyg, tekniker och metoder samt deras betydelse för arbetet. Undvik irrelevanta produktdetaljer.", "Document relevant tools, techniques, and methods and their significance. Avoid irrelevant product details."),
        minor("method", "method-validation", 18, "Validering", "Validation", "Förklara hur kvalitet och måluppfyllelse skulle bedömas och vilka kriterier som användes. Redovisa resultaten senare.", "Explain how quality and achievement were assessed and which criteria were used. Report outcomes later."),
        major("execution", 19, "Genomförande", "Execution", "Beskriv hur arbetet genomfördes i praktiken. Redovisa viktiga steg, lösningar och beslut i logisk ordning och fokusera på sådant som förklarar resultaten.", "Describe how the work was carried out in practice. Present important steps, solutions, and decisions logically, focusing on what explains the results.", true),
        minor("execution", "execution-overview", 20, "Översikt", "Overview", "Ge en översikt över genomförandets delar och deras samband. Låt läsaren förstå helheten innan detaljerna.", "Give an overview of the parts of the execution and their relationships. Help the reader understand the whole before details."),
        minor("execution", "execution-design", 21, "Design eller struktur", "Design or Structure", "Förklara den struktur eller design som arbetet resulterade i och motivera centrala val. Undvik detaljer utan betydelse för helheten.", "Explain the resulting structure or design and justify central choices. Avoid details that do not matter to the whole."),
        minor("execution", "execution-work", 22, "Implementation eller genomfört arbete", "Implementation or Work Performed", "Beskriv vad som faktiskt gjordes och vilka delar som blev färdiga. Skilj utfört arbete från planerade framtida möjligheter.", "Describe what was actually done and which parts were completed. Distinguish completed work from future possibilities."),
        minor("execution", "execution-decisions", 23, "Viktiga beslut", "Important Decisions", "Redovisa beslut och relevanta alternativ när de påverkar rapportens resultat eller användning. Motivera besluten med krav eller observationer.", "Report decisions and relevant alternatives when they affect results or use. Ground decisions in requirements or observations."),
        minor("execution", "execution-testing", 24, "Testning eller verifiering", "Testing or Verification", "Beskriv hur arbetet kontrollerades mot krav eller kriterier. Håll själva testupplägget skilt från testutfallet.", "Describe how the work was checked against requirements or criteria. Keep test design separate from outcomes."),
        major("results", 25, "Resultat", "Results", "Presentera resultaten sakligt och strukturerat. Beskriv vad som uppnåddes eller observerades och lämna den djupare tolkningen till diskussionen.", "Present results objectively and structurally. Describe what was achieved or observed and leave deeper interpretation to the discussion.", true),
        minor("results", "results-main", 26, "Huvudresultat", "Main Results", "Redovisa resultaten som svarar mot syfte och mål och ge läsaren tillräckligt underlag för att kontrollera dem.", "Present results addressing the purpose and objectives and provide enough evidence for the reader to assess them."),
        minor("results", "results-verification", 27, "Verifieringsresultat", "Verification Results", "Redovisa utfall från tester eller annan verifiering mot definierade kriterier. Ange begränsningar och avvikelser tydligt.", "Report testing or other verification outcomes against defined criteria. State limitations and deviations clearly."),
        minor("results", "results-deviations", 28, "Avvikelser", "Deviations", "Beskriv avvikelser från plan, krav eller förväntningar och deras observerade konsekvenser. Utelämna inte avvikelser som påverkar slutsatsen.", "Describe deviations from plans, requirements, or expectations and their observed consequences. Do not omit deviations affecting the conclusion.", false),
        major("discussion", 29, "Diskussion", "Discussion", "Tolka och diskutera resultaten i relation till syfte, mål, krav och vald metod. Beskriv styrkor, svagheter, osäkerheter och relevanta begränsningar.", "Interpret and discuss the results in relation to purpose, objectives, requirements, and method. Describe strengths, weaknesses, uncertainties, and relevant limitations.", true),
        minor("discussion", "discussion-results", 30, "Resultatdiskussion", "Result Discussion", "Förklara betydelsen av resultaten och skilj underbyggda slutsatser från möjliga tolkningar.", "Explain the significance of the results and distinguish supported conclusions from possible interpretations."),
        minor("discussion", "discussion-method", 31, "Metoddiskussion", "Method Discussion", "Bedöm hur metod och genomförande påverkade resultatens kvalitet. Föreslå inte förbättringar utan att förklara deras betydelse.", "Assess how method and execution affected result quality. Do not suggest improvements without explaining their significance."),
        minor("discussion", "discussion-limitations", 32, "Begränsningar", "Limitations", "Sammanfatta begränsningar och osäkerheter som påverkar rapportens räckvidd. Var konkret om vad läsaren bör vara försiktig med.", "Summarize limitations and uncertainties affecting the report's scope. Be concrete about what readers should treat cautiously."),
        minor("discussion", "discussion-lessons", 33, "Lärdomar", "Lessons Learned", "Beskriv lärdomar som kan förbättra liknande framtida arbete. Fokusera på överförbara insikter.", "Describe lessons that could improve similar future work. Focus on transferable insights.", false),
        major("conclusion", 34, "Slutsats", "Conclusion", "Sammanfatta vad rapporten visar i relation till syfte, mål och krav. Introducera inte nya resultat eller nya sakfrågor.", "Summarize what the report shows in relation to purpose, objectives, and requirements. Do not introduce new results or issues.", true),
        minor("conclusion", "conclusion-findings", 35, "Slutsatser", "Conclusions", "Formulera tydliga slutsatser som stöds av resultat och diskussion och håll dem inom rapportens avgränsning.", "State clear conclusions supported by the results and discussion and keep them within the report's scope."),
        minor("conclusion", "conclusion-recommendations", 36, "Rekommendationer", "Recommendations", "Ge rekommendationer endast när rapportens underlag motiverar dem. Ange vem eller vad de riktar sig till och varför.", "Give recommendations only when supported by the report. State who or what they address and why.", false),
        minor("conclusion", "conclusion-future", 37, "Framtida arbete", "Future Work", "Föreslå relevanta nästa steg som följer av begränsningar eller nya frågor. Håll förslagen realistiska och motiverade.", "Suggest relevant next steps arising from limitations or new questions. Keep suggestions realistic and justified.", false),
        major("references", 38, "Referenser", "References", "Lista använda källor konsekvent enligt vald referensstil och kontrollera att hänvisningar och poster stämmer överens.", "List used sources consistently in the selected citation style and check that citations and entries correspond.", true),
        major("appendices", 39, "Bilagor", "Appendices", "Placera kompletterande material som behövs för granskning men inte för huvudflödet här. Hänvisa tydligt från rapporten.", "Place supplementary material needed for review but not the main flow here. Refer to appendices clearly from the report.", false)
    });
}

QStringList validate(const DocumentTemplate& document)
{
    QList<QString> errors;
    if (document.id.isEmpty() || document.documentType.isEmpty()) errors << QStringLiteral("Built-in document template identity is missing");
    if (document.version < 1) errors << QStringLiteral("Built-in document template version is invalid");
    QSet<QString> ids;
    QSet<int> orders;
    QHash<QString, int> levels;
    for (const auto& item : document.sections) {
        if (item.id.isEmpty() || ids.contains(item.id)) errors << QStringLiteral("Document section IDs must be unique and non-empty");
        ids.insert(item.id);
        if (orders.contains(item.order)) errors << QStringLiteral("Document section order must be deterministic");
        orders.insert(item.order);
        if (item.level < 1 || item.level > 6) errors << QStringLiteral("Document section heading level is invalid: ") + item.id;
        if (item.titleSv.trimmed().isEmpty() || item.titleEn.trimmed().isEmpty()) errors << QStringLiteral("Document section titles must be bilingual: ") + item.id;
        if (item.guidanceSv.trimmed().isEmpty() || item.guidanceEn.trimmed().isEmpty()) errors << QStringLiteral("Document section guidance must be bilingual: ") + item.id;
        levels.insert(item.id, item.level);
    }
    for (const auto& item : document.sections) {
        if (!item.parentId.isEmpty() && !ids.contains(item.parentId)) errors << QStringLiteral("Document section parent is missing: ") + item.id;
        if (item.level == 1 && !item.parentId.isEmpty()) errors << QStringLiteral("Top-level document section has a parent: ") + item.id;
        if (item.level > 1 && item.parentId.isEmpty()) errors << QStringLiteral("Nested document section has no parent: ") + item.id;
        if (!item.parentId.isEmpty() && levels.value(item.parentId) != item.level - 1) errors << QStringLiteral("Document section hierarchy is invalid: ") + item.id;
        if (!item.parentId.isEmpty()) {
            const auto parent = std::find_if(document.sections.cbegin(), document.sections.cend(), [&](const DocumentSection& candidate) { return candidate.id == item.parentId; });
            if (parent != document.sections.cend() && parent->order >= item.order) errors << QStringLiteral("Document section parent must precede child: ") + item.id;
        }
    }
    return errors;
}

QJsonObject toJson(const DocumentTemplate& document)
{
    QJsonArray sections;
    for (const auto& item : document.sections) sections.append(sectionJson(item));
    return {{QStringLiteral("id"), document.id}, {QStringLiteral("documentType"), document.documentType},
            {QStringLiteral("version"), document.version}, {QStringLiteral("sections"), sections}};
}

QJsonObject manifest(bool thesisEnabled, const QString& thesisMode, const QString& thesisSourceId,
                     bool reportEnabled,
                     const QString& reportMode, const QString& reportSourceId,
                     const QString& thesisLanguage, const QString& reportLanguage)
{
    QJsonArray documents;
    const auto append = [&](const DocumentTemplate& document, bool enabled, const QString& documentMode, const QString& selectedSource, const QString& language) {
        QJsonObject value{{QStringLiteral("documentType"), document.documentType}, {QStringLiteral("mode"), documentMode},
                          {QStringLiteral("enabled"), enabled},
                          {QStringLiteral("templateId"), documentMode == QStringLiteral("aramf-default") ? document.id : QString()},
                          {QStringLiteral("templateVersion"), documentMode == QStringLiteral("aramf-default") ? document.version : 0},
                          {QStringLiteral("language"), language}, {QStringLiteral("guidanceAvailable"), true}};
        if (documentMode == QStringLiteral("aramf-default")) value.insert(QStringLiteral("sections"), toJson(document).value(QStringLiteral("sections")));
        else value.insert(QStringLiteral("sourceId"), selectedSource);
        documents.append(value);
    };
    append(thesis(), thesisEnabled, thesisMode, thesisSourceId, thesisLanguage);
    append(report(), reportEnabled, reportMode, reportSourceId, reportLanguage);
    return {{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("documents"), documents}};
}

QList<DocumentSection> tableOfContents(const DocumentTemplate& document)
{
    QList<DocumentSection> result = document.sections;
    std::sort(result.begin(), result.end(), [](const DocumentSection& left, const DocumentSection& right) {
        return left.order < right.order;
    });
    return result;
}

}
