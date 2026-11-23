#pragma once
/*
  GameObservatory.h
  ------------------------------------------------------------
  THE OBSERVATORY - a story and puzzle game about the astronomers of the
  Islamic world. 30 short missions in 5 chapters (the sky, instruments,
  events in the sky, great observatories, more minds). The characters and scenes are
  invented; the discoveries and the numbers are historical or scientific.

  Each mission: a story (with a drawing), one question (4 choices, or a dial
  you set with UP/DOWN), a hint after a wrong answer, then a fact card that
  goes into the CODEX. Stars: 3 = first try, 2 = second, 1 = after a hint.
  There is no way to lose: after 3 tries the answer is shown.

  Buttons: UP/DOWN move or change the value (in texts: previous/next page),
  MENU = OK / next, BACK = up one level. Progress is saved in flash ("obsS").
*/
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <math.h>
#include "UiCommon.h"
#include "Storage.h"
#include "NotoMono13.h"

namespace Ob {

constexpr int NM = 30;        // missions (the save keeps room for NMAX)
constexpr int NMAX = 32;
constexpr int COLS = 24;

struct Mis {
  const char *name;
  const char *story;
  const char *q;
  uint8_t kind;          // 0 choice, 1 dial, 2 choice with the drawing
  const char *opt[4];    // choices (dial: opt[0] = unit)
  int16_t right, dmin, dmax, dstart;
  uint8_t tol, dia;
  const char *hint;
  const char *fact;
};

static const Mis M[NM] = {
  {"NORTH STAR",
   "BAGHDAD, 830\nNight. Master Idris wakes you with a lamp: a caravan leaves at dawn and the guide must know how far north the road begins. Clouds are rolling in. Through one last gap you spot the star that never seems to move. You aim the quadrant: the sighting line settles at 36 degrees above the horizon.",
   "The Pole Star is 36 degrees high. What is the latitude here?", 0,
   {"36 degrees north", "54 degrees north", "18 degrees north", "72 degrees north"}, 0, 0, 0, 0, 0, 0,
   "The nearer the North Pole, the higher the Pole Star. At the Pole it is overhead; at the equator, on the horizon.",
   "The height of the Pole Star above the horizon equals your latitude. Travelers used it to know how far north they were, and astrolabes were engraved for one latitude. Baghdad lies near 33 degrees north."},
  {"NEW MOON",
   "CAIRO, 1000\nEvening of the 29th day. The whole street waits for the thin crescent that starts the new month. Idris stands on the roof, eyes on the western sky, while thick clouds drift over the sunset. 'Perhaps nobody will see it tonight,' he says. 'Then what must we count?' He turns to you.",
   "The Moon needs 29.53 days from new moon to new moon. A month lasts...", 0,
   {"always 29 days", "29 or 30 days", "28 to 31 days", "always 30 days"}, 1, 0, 0, 0, 0, 1,
   "Months cannot hold a fraction of a day, and 29.53 lies between two whole numbers.",
   "A lunar month is about 29.53 days, so months have 29 or 30 days and a lunar year about 354 days. When clouds hide the crescent on the 29th evening, the month is completed at 30 days. Astronomers could also compute the new moon long before it became visible."},
  {"THE SHADOW",
   "DAMASCUS, 1100\nThe muezzin needs the exact middle of the day, and the sky is bright but the clock is broken. In the courtyard Idris plants a straight stick and asks you to draw its shadow four times between morning and afternoon. One of the four drawings shows the moment the sun stands highest.",
   "Which stick is midday?", 2,
   {"A", "B", "C", "D"}, 2, 0, 0, 0, 0, 2,
   "Least shadow at midday.",
   "The shadow of a stick is shortest at local noon, when the sun is highest. Its direction then marks the north-south line. With shadows, astronomers fixed noon, the direction of north and the length of the day. Al-Biruni wrote a whole book on shadows around 1021."},
  {"ASTROLABE",
   "ALEPPO, 960\nIn a small workshop a brass astrolabe lies on the table, polished by a woman skilled in this craft. 'Tonight you must read the time from a star,' she says. 'First measure the star's height with the sighting arm. Then the star map on top must be turned until it matches the sky.' The lamp flickers. Which part will you turn?",
   "Which part do you turn to match the sky?", 0,
   {"The rete (star map)", "The plate (altitudes)", "The mater (the body)", "The ring (to hang it)"}, 0, 0, 0, 0, 0, 3,
   "It is the cut-out part with pointers for bright stars. It turns above the plate like the sky above you.",
   "The astrolabe is a pocket model of the sky. The rete carries the star pointers and turns above a plate engraved for one latitude. It gives the time, the position of stars and the height of the sun. In the 10th century Maryam al-Asturlabi made astrolabes in Aleppo."},
  {"THE DIRECTION",
   "CAIRO, 1100\nDawn is close. A new mosque needs its wall laid out, and the builders wait for the astronomer. Idris unrolls a map: Mecca lies about 960 km south and 860 km east of Cairo. 'Turn the compass rose,' he says. 'Give the bearing, measured clockwise from north. Not exact, but close.'",
   "Set the direction of Mecca.", 1,
   {"degrees", "", "", ""}, 136, 0, 359, 90, 8, 4,
   "North 0, east 90, south 180. South-east is about halfway.",
   "A direction on the globe follows the shortest route, a great circle. Astronomers solved it with spherical trigonometry and published direction tables for many cities. From Cairo the bearing is about 136 degrees, south-east."},
  {"EARTH SIZE",
   "NANDANA, 1020\nOn a bare hill, Idris climbs to the top with a sighting tube while you wait below. From up there the plain ends in a razor-sharp horizon, and it lies a tiny angle BELOW eye level. With that angle and the hill's height he will deduce the size of the whole Earth. 'What if the Earth were smaller?' he asks.",
   "On the same hill, a smaller Earth would make the horizon seem...", 0,
   {"lower (bigger dip)", "higher", "exactly the same", "impossible to see"}, 0, 0, 0, 0, 0, 5,
   "A small ball curves away faster than a big one, so the horizon falls further below your eye.",
   "The dip of the horizon seen from a height depends on the Earth's radius. Al-Biruni used this idea near Nandana in the early 11th century and found about 6,340 km, within 1 percent of the modern 6,378 km at the equator. He needed only a hill, a sighting tube and trigonometry."},
  {"DARK MOON",
   "BAGHDAD, 900\nThe full moon rises, then slowly turns dark copper. In the market people drop their baskets and shout. Idris walks calmly through the crowd with his notebook. 'Last month I wrote that this would happen tonight,' he says. He turns to you: 'Explain why it can only happen on a night like this.'",
   "A lunar eclipse can only happen when the Moon is...", 0,
   {"full", "new", "a thin crescent", "at first quarter"}, 0, 0, 0, 0, 0, 6,
   "The Earth must stand between the Sun and the Moon, in a straight line.",
   "In a lunar eclipse the Moon crosses the Earth's shadow, so it happens at full moon (a solar eclipse at new moon). Eclipses repeat after about 18 years 11 days, the Saros, which helps to predict them. Ibn Yunus of Cairo recorded many eclipses around 1000; his records are still studied today."},
  {"NEW STAR",
   "CAIRO, 1006\nSpring. A dazzling light hangs low in the southern sky, brighter than anything you have seen at night. The city is uneasy. Idris takes his pen. 'Do not be afraid: observe. People a thousand years from now will want to know exactly what we saw.' He looks at you. 'What should we write?'",
   "Which note helps scientists most, a thousand years later?", 0,
   {"A frightening sign!", "Date, site, brightness", "Only its color", "A drawing, no date"}, 1, 0, 0, 0, 0, 7,
   "Science needs measurements others can check: when, where and how bright.",
   "In 1006 a supernova, the brightest star-like event ever recorded, shone for months. Ali ibn Ridwan in Cairo wrote that it was two and a half to three times as large as Venus, with light a little more than a quarter of the Moon's. Today astronomers use such records to study its remains, about 7,000 light-years away."},
  {"LITTLE CLOUD",
   "SHIRAZ, 964\nIn his study an astronomer is revising the old star catalogue, and you read out the constellation of the Chained Woman. 'Look,' says Idris, 'among these stars is something that is not a star: a small cloud, always in the same place, even on the clearest night.' It is no comet, it does not move. What could it be?",
   "That small cloud is in fact...", 0,
   {"a galaxy of stars", "a cloud in our air", "a comet", "one very big star"}, 0, 0, 0, 0, 0, 8,
   "It never moves among the stars and looks the same every clear night, so it is far beyond our air.",
   "In 964 al-Sufi wrote his Book of Fixed Stars and described this small cloud: the earliest known record of the Andromeda Galaxy. It holds hundreds of billions of stars and lies about 2.5 million light-years away, one of the farthest things you can see with the naked eye."},
  {"STAR NAMES",
   "A ROOFTOP, 1000\nA warm night. Idris points at an orange star rising low. 'Al-Dabaran,' he says, 'the Follower. Its name tells what it does every night: it rises a little after something small and glittering.' He grins. 'Guess what it follows.'",
   "Aldebaran, 'the Follower', follows...", 0,
   {"The Pleiades cluster", "The Pole Star", "The planet Venus", "The Sun"}, 0, 0, 0, 0, 0, 9,
   "A tiny group of glittering stars, often called the Seven Sisters.",
   "Many star names come from Arabic: Altair (the flying eagle), Aldebaran (the follower), Fomalhaut (mouth of the fish), Deneb (the tail). Words like zenith, azimuth and nadir also come from Arabic astronomy. You may have visited some of these systems in SKY TRADER."},
  {"WHEEL IN WHEEL",
   "MARAGHA, 1260\nOn a hill above Maragha a great observatory is rising, with a huge library. A young engineer shows you a riddle of Nasir al-Din al-Tusi's team: a small wheel rolls inside a big wheel, and one point of its rim travels along a perfectly straight line. 'The small wheel must have exactly the right size,' she says.",
   "The small wheel's radius, compared to the big wheel's, is...", 0,
   {"half", "one third", "the same", "double"}, 0, 0, 0, 0, 0, 10,
   "It must fit inside the big wheel, and exactly one half is the magic ratio.",
   "This is the Tusi couple, described by Nasir al-Din al-Tusi at the Maragha observatory, founded around 1259. It turns circular motion into a back-and-forth line, and helped to build better models of the planets. A similar device appears later in the work of Copernicus."},
  {"THE YEAR",
   "ISFAHAN, 1079\nThe Sultan wants a calendar that never drifts from the seasons. Astronomers, among them Omar Khayyam, are called to the court. Idris explains the problem: a leap day every 4 years makes the year 365.25 days, but the seasons repeat every 365.2422 days. Each year the calendar runs 0.0078 days too long.",
   "After 1000 years, how many days is it off? Set the number.", 1,
   {"days", "", "", ""}, 8, 0, 30, 0, 1, 11,
   "Multiply 0.0078 days by 1000 years.",
   "One day of error every 128 years adds up: nearly 8 days in 1000 years. In 1079 the astronomers of Sultan Malik-Shah, among them Omar Khayyam, built the Jalali calendar, far more precise. Today's Iranian solar calendar is rooted in it."},
  {"GIANT SEXTANT",
   "SAMARKAND, 1430\nUlugh Beg's observatory rises like a tower, and in a deep trench lies a giant arc, about 36 meters in radius. You stare. His astronomers measured the year as 365 days 6 h 10 min 8 s and mapped more than a thousand stars. 'Everything depends on this size,' says Idris. 'Can you tell why?'",
   "Why build such a huge sextant?", 0,
   {"to read finer angles", "to impress visitors", "to hold more people", "to catch more light"}, 0, 0, 0, 0, 0, 12,
   "On a bigger arc one degree is spread over a longer stretch that you can divide into tiny marks.",
   "A larger arc lets you read smaller angles. Ulugh Beg's team finished the Zij-i Sultani star catalogue in 1437 with 1,018 stars, and measured the year within 58 seconds of the modern value. They also found the tilt of the Earth's axis: 23.5 degrees."},
  {"DARK ROOM",
   "CAIRO, 1015\nA scholar has shut himself in a dark room, with one tiny hole cut in the shutter. On the opposite wall a faint, glowing picture of the sunny street appears, with people walking... on their heads! The picture is upside down. Idris asks whether it is magic, and whether you can explain it.",
   "Why is it upside down?", 2,
   {"Light goes straight", "The wall is tilted", "The Earth is round", "A mirror hides inside"}, 0, 0, 0, 0, 0, 14,
   "Rays cross in the hole.",
   "Ibn al-Haytham (Alhazen) wrote his Book of Optics around 1011-1021 in Cairo. He showed that we see because light enters the eye, not because rays leave it, studied this dark room (camera obscura) and tested his ideas by experiments. Many call him a pioneer of the scientific method."},
  {"THE UNKNOWN",
   "BAGHDAD, 820\nAt the House of Wisdom a merchant is stuck on an inheritance: a square plus ten of its own roots makes 39. Idris smiles. 'Al-Khwarizmi solves it with a picture. Draw the square, add two strips of 5 along its sides, and fill the corner with a small square of 25. The whole figure is a bigger square.'",
   "Find x, the side of the first square.", 1,
   {"", "", "", ""}, 3, 0, 10, 0, 0, 15,
   "Area 39+25=64: side 8. So x+5=8.",
   "Al-Khwarizmi (about 780-850) wrote the first book on al-jabr, the origin of the word algebra, and his own name gave us the word algorithm. He solved equations with pictures like this one. His book on Indian numerals helped bring our decimal system, with zero, to Europe."},
  {"SECRET LETTER",
   "BAGHDAD, 850\nA sealed letter reaches Idris at midnight. It begins with nonsense: WKH VWDU LV ULVLQJ. 'A secret message,' he whispers. 'Every letter was pushed forward by the same number of steps. In ordinary writing E is the most common letter, and in the long text that follows, H is the most common.'",
   "Set the shift from E to H.", 1,
   {"steps", "", "", ""}, 3, 1, 25, 1, 0, 16,
   "Count the steps: E to F is 1, then G, then H.",
   "The message reads: THE STAR IS RISING. In the 9th century al-Kindi of Baghdad described frequency analysis, the earliest known way to break a simple secret code: in a long text some letters appear far more often than others."},
  {"SOUTH ON TOP",
   "PALERMO, 1154\nKing Roger II of Sicily asked the geographer al-Idrisi for a map of the whole known world. Idris unrolls a copy and frowns. 'Something is odd. Arabia is above and Europe below. Look at the compass arrow in the corner of the map.'",
   "Which side is the top?", 2,
   {"South", "North", "East", "West"}, 0, 0, 0, 0, 0, 17,
   "The arrow N points down.",
   "Al-Idrisi's world map (1154) has south at the top, like many maps of the Islamic world at the time. He built it from the reports of travelers and merchants, for a book called 'The Pleasure of Him Who Longs to Cross the World'. It remained a reference for a long time."},
  {"THE LUNGS",
   "CAIRO, 1240\nIn a hospital ward, a student repeats what the old books say: blood crosses from the right side of the heart to the left through tiny holes in the wall between them. Idris shakes his head. 'Ibn al-Nafis looked and found no holes. So where does the blood go?'",
   "Blood then flows to the", 2,
   {"lungs", "liver", "brain", "stomach"}, 0, 0, 0, 0, 0, 18,
   "Where we breathe air.",
   "Around 1242 Ibn al-Nafis, a physician in Cairo, wrote that blood passes from the right side of the heart through the lungs, where it meets air, and returns to the left side: the first known description of the lesser circulation, about three centuries before Europe described it."},
  {"CRANK AND ROD",
   "DIYARBAKIR, 1206\nThe workshop of al-Jazari is full of water clocks, fountains and singing automata. One machine must turn the spinning of a water wheel into the push and pull of a pump. Idris holds up a wheel with an arm fixed near its rim, and a rod attached to the arm. 'Which parts do the job?'",
   "Which parts do the job?", 2,
   {"A crank and rod", "A pulley", "A spring", "A wedge"}, 0, 0, 0, 0, 0, 19,
   "Like a bicycle pedal.",
   "Al-Jazari's Book of Knowledge of Ingenious Mechanical Devices (1206) describes water clocks, fountains and automata, and an early crank-and-rod mechanism that turns rotation into a back-and-forth push. The same idea moves the pistons of engines today."},
  {"PI POLYGONS",
   "SAMARKAND, 1424\nAl-Kashi sits among bundles of paper, filling page after page with figures. 'He wants the circumference of a circle with more digits than anyone has ever found,' says Idris. 'His method: draw a polygon inside the circle and compute its perimeter. A hexagon is rough, a 12-sided shape is closer.' He asks you for the rule.",
   "To get closer to the circle, the polygon needs...", 0,
   {"more sides", "fewer sides", "longer sides", "a square shape"}, 0, 0, 0, 0, 0, 20,
   "With more sides, the corners get closer to the circle.",
   "In 1424 al-Kashi computed 2 pi to 16 correct decimal places, using a polygon with 805,306,368 sides. It was the most precise value for nearly two centuries. His book 'The Key to Arithmetic' (1427) teaches methods used by astronomers and builders."},
  {"RAINBOW",
   "TABRIZ, 1300\nAfter a storm the sun breaks through and a rainbow arches over the city. Idris carries a round glass flask of water into a dark room and lets a thin beam of sunlight enter it. 'Kamal al-Din al-Farisi did exactly this,' he says. 'The flask is a giant drop of something. Watch what the light does inside.'",
   "A rainbow is sunlight bent and reflected inside...", 0,
   {"raindrops", "the Moon", "fine dust", "the sea"}, 0, 0, 0, 0, 0, 21,
   "The flask of water stands for something that falls from the sky.",
   "Kamal al-Din al-Farisi (about 1260-1320) and his teacher Qutb al-Din al-Shirazi explained the rainbow: sunlight bends entering a raindrop, bounces once inside and bends again on the way out, splitting into colors. They tested it with a glass sphere of water in a dark room."},
  {"AMICABLE",
   "BAGHDAD, 880\nIn the House of Wisdom, the students of Thabit ibn Qurra whisper about two numbers that are 'friends': 220 and 284. Idris leans in. 'Add the divisors of one, not counting the number itself, and you get the other. The divisors of 220 are 1, 2, 4, 5, 10, 11, 20, 22, 44, 55 and 110.'",
   "Add those 11 numbers. What do you get?", 1,
   {"", "", "", ""}, 284, 0, 400, 200, 0, 22,
   "1+2+4+5+10 = 22. 11+20+22 = 53. 44+55+110 = 209.",
   "Thabit ibn Qurra (836-901), a scholar of Baghdad, found a rule that produces pairs of amicable numbers such as 220 and 284. Later Arab mathematicians also found larger pairs, such as 17,296 and 18,416."},
  {"MONSOON",
   "THE INDIAN OCEAN, 900\nIn the port of Basra, merchants wait for the wind, not for the tide. Their ships will sail to India and beyond. A captain tells you: 'We leave with the summer wind and come home with the winter wind. It carries us both ways.' Idris smiles. 'Why can ships not simply sail whenever they like?'",
   "Why do ships go out and come back in different seasons?", 0,
   {"The winds reverse", "Ships go faster", "The sea shrinks", "Ports close in summer"}, 0, 0, 0, 0, 0, 23,
   "Sailors depended on winds that blow one way in summer and the opposite way in winter.",
   "The monsoon winds blow from the southwest in summer and from the northeast in winter. For centuries sailors used them to cross the Indian Ocean between Arabia, East Africa, India and China, carrying goods, books and ideas. Ibn Majid wrote a famous navigation guide in 1490."},
  {"FIRST SCHOOL",
   "FEZ, 859\nA woman named Fatima al-Fihri has inherited a fortune. Idris tells you that she chooses neither jewels nor a palace: she buys land and builds a mosque with a place for study, and teachers soon arrive from far away. 'And what did that school become, over the centuries?'",
   "That school became...", 0,
   {"a living university", "a palace", "a market", "a lighthouse"}, 0, 0, 0, 0, 0, 24,
   "Look for the answer that has to do with learning.",
   "Fatima al-Fihri founded al-Qarawiyyin in Fez in 859. It grew into a university often called the oldest in the world still in operation, and its library keeps precious manuscripts. Many students and scholars traveled to Fez to learn there."},
  {"PAPER",
   "SAMARKAND, 751\nIn the market, Idris unrolls a smooth white sheet. 'Scholars used to write on papyrus or on costly parchment made of animal skin,' he says. 'Tradition tells that papermakers from China taught the craft here: rags and plant fibres, pressed and dried.' Within a few decades the secret has reached Baghdad.",
   "Why did paper change the world of knowledge?", 0,
   {"Books became cheaper", "Books became heavier", "Ink stopped working", "Libraries closed"}, 0, 0, 0, 0, 0, 25,
   "What happens when something becomes easy to make?",
   "The first paper mills of the Islamic world appeared in Samarkand and Baghdad (about 794). Paper then spread to Cairo, Fez and Spain, and from Spain to Europe. Cheaper books meant more copies, bigger libraries and more readers: whole streets of Baghdad were filled with the shops of copyists and booksellers."},
  {"EMPTY PLACE",
   "BAGHDAD, 825\nIdris draws a small circle on his slate and says nothing. 'In the new numbers from India, this circle marks an empty place, so 305 and 35 are not the same. Al-Khwarizmi wrote the rules for these numbers. Merchants called the empty place sifr, which just means empty.' He asks which words came from it.",
   "Which two words come from sifr?", 0,
   {"zero and cipher", "sugar and sofa", "magic and sand", "paper and pen"}, 0, 0, 0, 0, 0, 26,
   "One is the number of apples left after you eat them all.",
   "The Arabic sifr became zephirum in Latin, and then both zero and cipher. Fibonacci's Book of Calculation (1202), written after he learned the numerals in North Africa, introduced them to Europe, where calculating with them was much easier than with Roman numerals."},
  {"THE TRIANGLE",
   "BAGHDAD, 1010\nAl-Karaji studies how to expand (a+b) multiplied by itself many times. Idris draws a triangle of numbers: the edges are 1 and every other number is the sum of the two numbers above it. 'Look at the rows,' he says. 'One more row, and the numbers will tell you the coefficients for the next power.'",
   "Next row: 1 5 ? ? 5 1. Find ?", 1,
   {"", "", "", ""}, 10, 0, 20, 5, 0, 27,
   "Add the two numbers above: 4 + 6.",
   "The triangle appeared in the lost book of al-Karaji (about 1000) and is cited by al-Samaw'al (about 1150). Omar Khayyam also knew it. Europe later named it after Pascal (1654). Each row gives the coefficients of (a+b) raised to a power."},
  {"THE HOSPITAL",
   "CAIRO, 1284\nThe new al-Mansuri hospital has fountains, wards for different illnesses, a pharmacy, a library and a lecture hall. A guide tells you the sultan ordered that treatment be free until the patient is well. Idris nods: 'Hospitals, called bimaristans, were already known in Baghdad and Damascus.'",
   "Who could be treated there?", 0,
   {"Everyone, rich or poor", "Only the sultan", "Only soldiers", "Only travelers"}, 0, 0, 0, 0, 0, 28,
   "Read the story again: nobody was turned away.",
   "Bimaristans had wards for different illnesses, pharmacies, libraries and teaching rooms, and they trained doctors. The al-Mansuri hospital in Cairo, founded in 1284, treated patients free of charge and stayed in service for centuries."},
  {"TWO FEVERS",
   "RAYY, 900\nIn the hospital of Rayy, the physician al-Razi writes careful notes on every patient: when the fever begins, the look of the spots, how long they last. 'Two illnesses cover a child's skin with spots, and everyone mixes them up,' he says. 'I want to tell smallpox from measles.' Idris asks how he will succeed.",
   "How can he tell them apart?", 0,
   {"By careful observation", "By guessing", "By the weather only", "By asking the stars"}, 0, 0, 0, 0, 0, 29,
   "He noted every detail, case after case.",
   "Al-Razi (865-925) wrote one of the first clear descriptions of smallpox and measles as two different diseases. He relied on observation, kept case notes and even doubted the ancient authorities when his patients disagreed with the old books. He directed hospitals in Rayy and Baghdad."},
  {"THE SURGEON",
   "CORDOBA, 1000\nThe court physician al-Zahrawi is drawing on a large sheet: scalpels, forceps, hooks, cauteries, even a tool to remove arrows. 'A surgeon must know his tools as well as the body,' says Idris. 'This book will show about two hundred of them.' He asks why the doctor draws each one.",
   "Why draw every instrument?", 0,
   {"So students can learn", "To decorate the book", "To sell the tools", "To frighten patients"}, 0, 0, 0, 0, 0, 30,
   "A picture teaches better than a long text.",
   "Al-Zahrawi (Abulcasis, 936-1013) wrote al-Tasrif, a medical encyclopaedia in 30 parts. Its surgery chapter shows about 200 instruments, and describes stitches with catgut and the tying of blood vessels. Translated into Latin in the 12th century, it served European surgeons for centuries."},
};

static const char *const ABOUT =
  "THE OBSERVATORY\nThe characters and scenes are invented. The discoveries are real: al-Biruni, al-Sufi, Ibn Ridwan, Ibn Yunus, al-Tusi, Omar Khayyam and Ulugh Beg, with the astronomers, physicians and craftsmen of many cities. Facts were checked with MacTutor and encyclopedia sources. Look up at the sky tonight.";
static const char *const FINALE =
  "MASTER OBSERVER\nIdris smiles. 'You have followed every discovery, from the Pole Star to the crank of al-Jazari. The world was the same for all of them. What changed is that people looked closely, kept careful records and shared them.' He hands you the quadrant. 'Now it is your turn to look up.'";

enum Ph : uint8_t { PH_MENU = 0, PH_LIST, PH_STORY, PH_ASK, PH_FACT, PH_CODEX, PH_ABOUT, PH_END };
RTC_DATA_ATTR uint8_t phase = PH_MENU, sel = 0, page = 0, cur = 0, tries = 0, wrong = 0, fromCodex = 0, loaded = 0;
RTC_DATA_ATTR int16_t dv = 0;
RTC_DATA_ATTR uint8_t st[NM];

inline void load() {
  if (loaded) return;
  uint8_t b[NMAX + 1];
  memset(st, 0, sizeof(st));
  if (QWStorage::loadBlob("obsS", b, sizeof(b)) && b[0] == 0x4F) { for (int i = 0; i < NM; i++) st[i] = b[i + 1] > 3 ? 3 : b[i + 1]; }
  else if (QWStorage::loadBlob("obsS", b, 14) && b[0] == 0x4F) { for (int i = 0; i < 13; i++) st[i] = b[i + 1] > 3 ? 3 : b[i + 1]; }   // first version: 13 missions
  loaded = 1;
}
inline void save() { uint8_t b[NMAX + 1]; memset(b, 0, sizeof(b)); b[0] = 0x4F; memcpy(b + 1, st, NM); QWStorage::saveBlob("obsS", b, sizeof(b)); }
inline int total() { int t = 0; for (int i = 0; i < NM; i++) t += st[i]; return t; }
inline bool unlocked(int i) { return i == 0 || st[i - 1] > 0; }
inline bool allDone() { for (int i = 0; i < NM; i++) if (!st[i]) return false; return true; }

// ---------------------------------------------------------------- text
static char buf[760];
static uint16_t ls[48], ll[48];
static int nl = 0;
inline void wrap(const char *t) {
  nl = 0;
  int i = 0, n = (int)strlen(t);
  while (i < n && nl < 47) {
    while (i < n && t[i] == ' ') i++;
    int s = i, last = -1, j = i;
    while (j < n && j - s < COLS && t[j] != '\n') { if (t[j] == ' ') last = j; j++; }
    int e;
    if (j >= n || t[j] == '\n') e = j;
    else if (t[j] == ' ') e = j;
    else e = last > s ? last : j;
    ls[nl] = (uint16_t)s; ll[nl] = (uint16_t)(e - s); nl++;
    i = e; if (i < n && (t[i] == '\n' || t[i] == ' ')) i++;
  }
}
inline void pageRange(bool dia, int p, int &a, int &n) {
  int c0 = dia ? 5 : 11;
  if (p == 0) { a = 0; n = nl < c0 ? nl : c0; }
  else { a = c0 + (p - 1) * 11; n = nl - a; if (n > 11) n = 11; if (n < 0) n = 0; }
}
inline int nPages(bool dia) { int c0 = dia ? 5 : 11; return nl <= c0 ? 1 : 1 + (nl - c0 + 10) / 11; }

// ---------------------------------------------------------------- drawing helpers
inline void setup13(Adafruit_GFX &d) { d.setTextSize(1); d.setTextColor(qwInk()); d.setFont(&NotoMono13); }
inline void txtN(Adafruit_GFX &d, const char *s, int n, int16_t x, int16_t y) {
  char b[40]; if (n > 39) n = 39; strncpy(b, s, n); b[n] = 0; d.setCursor(x, y); d.print(b);
}
inline void header(Adafruit_GFX &d, const char *l, const char *r) {
  setup13(d);
  if (l) txtN(d, l, 16, 6, 12);
  if (r) { int n = (int)strlen(r); txtN(d, r, n, 194 - 8 * n, 12); }
  d.drawFastHLine(4, 15, 192, qwInk());
}
inline void footer(Adafruit_GFX &d, const char *l, const char *r) {
  setup13(d);
  if (l) txtN(d, l, 20, 6, 197);
  if (r) { int n = (int)strlen(r); txtN(d, r, n, 194 - 8 * n, 197); }
}
inline void sky(Adafruit_GFX &d, int seed, int n, int y0, int y1) {
  uint32_t h = 2166136261u ^ (uint32_t)seed;
  for (int i = 0; i < n; i++) {
    h = h * 16777619u + 7u; int x = 4 + (int)((h >> 8) % 192);
    h = h * 16777619u + 7u; int y = y0 + (int)((h >> 8) % (uint32_t)(y1 - y0));
    d.drawPixel(x, y, qwInk());
    if ((h >> 4) % 5 == 0) { d.drawPixel(x + 1, y, qwInk()); d.drawPixel(x, y + 1, qwInk()); }
  }
}
inline void rays(Adafruit_GFX &d, int cx, int cy, int r0, int r1, int n, uint16_t c) {
  for (int k = 0; k < n; k++) {
    float a = 6.2832f * k / n;
    d.drawLine(cx + (int)lroundf(r0 * cosf(a)), cy + (int)lroundf(r0 * sinf(a)), cx + (int)lroundf(r1 * cosf(a)), cy + (int)lroundf(r1 * sinf(a)), c);
  }
}
inline void crescent(Adafruit_GFX &d, int cx, int cy, int r, int off) {
  d.fillCircle(cx, cy, r, qwInk());
  if (off < r + r) d.fillCircle(cx + off, cy - 1, r, qwPaper());
}
// arrow from (x0,y0) at screen angle a (radians)
inline void arrow(Adafruit_GFX &d, int x0, int y0, float a, int len, int hs) {
  int x1 = x0 + (int)lroundf(len * cosf(a)), y1 = y0 + (int)lroundf(len * sinf(a));
  d.drawLine(x0, y0, x1, y1, qwInk());
  d.drawLine(x0 + 1, y0, x1 + 1, y1, qwInk());
  float b1 = a + 2.7f, b2 = a - 2.7f;
  d.drawLine(x1, y1, x1 + (int)lroundf(hs * cosf(b1)), y1 + (int)lroundf(hs * sinf(b1)), qwInk());
  d.drawLine(x1, y1, x1 + (int)lroundf(hs * cosf(b2)), y1 + (int)lroundf(hs * sinf(b2)), qwInk());
}

// ---------------------------------------------------------------- drawings (box x 0..200, y 18..98)
inline void drawDia(Adafruit_GFX &d, int id, int v) {
  uint16_t ink = qwInk(), pap = qwPaper();
  switch (id) {
    case 0: {   // quadrant sighting the Pole Star at 36 degrees
      int cx = 40, cy = 92, R = 66;
      for (int a = 0; a <= 90; a++) { float r = a * 0.017453f; d.drawPixel(cx + (int)lroundf(R * cosf(r)), cy - (int)lroundf(R * sinf(r)), ink); d.drawPixel(cx + (int)lroundf((R - 1) * cosf(r)), cy - (int)lroundf((R - 1) * sinf(r)), ink); }
      for (int a = 0; a <= 90; a += 5) { float r = a * 0.017453f; int l = a % 10 == 0 ? 8 : 4; d.drawLine(cx + (int)lroundf((R - l) * cosf(r)), cy - (int)lroundf((R - l) * sinf(r)), cx + (int)lroundf(R * cosf(r)), cy - (int)lroundf(R * sinf(r)), ink); }
      d.drawFastHLine(cx, cy, R, ink); d.drawFastVLine(cx, cy - R, R, ink);
      float t = 36 * 0.017453f; int ex = cx + (int)lroundf(104 * cosf(t)), ey = cy - (int)lroundf(104 * sinf(t));
      d.drawLine(cx, cy, ex, ey, ink); d.drawLine(cx, cy - 1, ex, ey - 1, ink);
      d.drawFastHLine(cx + R + 4, cy, 70, ink);
      int sx = ex + 9, sy = ey - 7; d.fillCircle(sx, sy, 3, ink); rays(d, sx, sy, 5, 9, 8, ink);
      setup13(d); d.setFont(nullptr); d.setTextSize(1); d.setCursor(cx + 24, cy - 12); d.print("36"); d.setFont(&NotoMono13);
      break;
    }
    case 1: {   // crescents growing; clouds on the first
      int xs[5] = {24, 62, 100, 138, 176};
      int off[5] = {10, 7, 4, 2, 30};
      for (int k = 0; k < 5; k++) crescent(d, xs[k], 58, 13, off[k]);
      int cc[4][3] = {{8, 62, 9}, {18, 56, 11}, {30, 58, 10}, {38, 64, 8}};   // cloud over the first moon
      for (int c = 0; c < 4; c++) d.fillCircle(cc[c][0], cc[c][1], cc[c][2] + 1, ink);
      for (int c = 0; c < 4; c++) d.fillCircle(cc[c][0], cc[c][1], cc[c][2], pap);
      d.drawFastHLine(0, 94, 200, ink);
      break;
    }
    case 2: {   // four sticks with shadows A..D
      for (int x = 4; x < 196; x += 4) d.drawPixel(x, 90, ink);
      int len[4] = {22, 12, 3, 14}, dir[4] = {-1, -1, 1, 1};
      for (int k = 0; k < 4; k++) {
        int x0 = 10 + k * 47 + 24, hgt = 24;
        d.drawFastVLine(x0, 88 - hgt, hgt, ink); d.drawFastVLine(x0 + 1, 88 - hgt, hgt, ink);
        int tx = x0 + dir[k] * len[k];
        for (int w = 0; w < 3; w++) d.drawLine(x0, 87 - w, tx, 87 - w, ink);
        float th = atanf((float)hgt / len[k]);
        int sx = x0 - dir[k] * (int)lroundf(36 * cosf(th)), sy = 88 - hgt - (int)lroundf(36 * sinf(th)); if (sy < 24) sy = 24;
        d.drawCircle(sx, sy, 3, ink); rays(d, sx, sy, 5, 8, 6, ink);
      }
      break;
    }
    case 3: {   // astrolabe
      int cx = 100, cy = 62;
      d.drawCircle(cx, cy, 34, ink); d.drawCircle(cx, cy, 33, ink); d.drawCircle(cx, cy, 28, ink);
      d.fillCircle(cx, cy - 38, 4, ink); d.fillCircle(cx, cy - 38, 2, pap);
      d.drawCircle(cx, cy, 20, ink); d.drawCircle(cx, cy, 10, ink);
      d.drawFastHLine(cx - 28, cy, 56, ink); d.drawFastVLine(cx, cy - 28, 56, ink);
      d.drawCircle(cx + 2, cy - 4, 17, ink);
      for (int k = 0; k < 6; k++) { float a = 0.9f + k * 1.0f; int px = cx + (int)lroundf(24 * cosf(a)), py = cy + (int)lroundf(24 * sinf(a)); d.fillTriangle(px, py - 3, px - 2, py + 2, px + 2, py + 2, ink); d.drawLine(px, py, cx + (int)lroundf(14 * cosf(a)), cy + (int)lroundf(14 * sinf(a)), ink); }
      d.drawLine(cx - 30, cy + 18, cx + 30, cy - 18, ink); d.drawLine(cx - 30, cy + 19, cx + 30, cy - 17, ink);
      break;
    }
    case 4: {   // compass rose with the bearing arrow
      int cx = 100, cy = 58, R = 30;
      d.drawCircle(cx, cy, R, ink); d.drawCircle(cx, cy, R - 1, ink);
      for (int k = 0; k < 36; k++) { float a = k * 0.17453f; int l = k % 9 == 0 ? 8 : 3; d.drawLine(cx + (int)lroundf((R - l) * sinf(a)), cy - (int)lroundf((R - l) * cosf(a)), cx + (int)lroundf(R * sinf(a)), cy - (int)lroundf(R * cosf(a)), ink); }
      setup13(d); d.setFont(nullptr); d.setTextSize(1);
      d.setCursor(cx - 2, cy - R - 11); d.print("N"); d.setCursor(cx + R + 5, cy - 3); d.print("E"); d.setCursor(cx - 2, cy + R + 4); d.print("S"); d.setCursor(cx - R - 11, cy - 3); d.print("W");
      d.setFont(&NotoMono13);
      float a = (v - 90) * 0.017453f; arrow(d, cx, cy, a, 24, 6);
      d.fillCircle(cx, cy, 3, ink);
      break;
    }
    case 5: {   // hill, horizon dip, earth arc
      for (int a = 0; a < 360; a++) { float r = a * 0.017453f; int x = 100 + (int)lroundf(100 * cosf(r)), y = 160 + (int)lroundf(100 * sinf(r)); if (y >= 20 && y <= 97) { d.drawPixel(x, y, ink); d.drawPixel(x, y + 1, ink); } }
      d.fillTriangle(88, 62, 100, 44, 112, 62, ink);
      d.drawFastHLine(100, 44, 90, ink);
      for (int x = 104; x < 190; x += 6) d.drawPixel(x, 44, pap);
      float ang = acosf(100.0f / 116.0f); int tx = 100 + (int)lroundf(62 * cosf(ang)), ty = 44 + (int)lroundf(62 * sinf(ang));
      d.drawLine(100, 44, tx, ty, ink);
      d.fillCircle(100, 42, 2, ink);
      break;
    }
    case 6: {   // Sun - Earth - Moon in line
      d.fillCircle(28, 58, 18, ink); rays(d, 28, 58, 21, 27, 14, ink);
      d.drawCircle(110, 58, 9, ink); d.fillCircle(110, 58, 9, ink);
      d.drawLine(110, 49, 195, 54, ink); d.drawLine(110, 67, 195, 62, ink);
      for (int x = 118; x < 192; x += 3) for (int y = 52; y < 65; y += 3) { int top = 49 + (x - 110) * 5 / 85, bot = 67 - (x - 110) * 5 / 85; if (y > top + 1 && y < bot - 1) d.drawPixel(x, y, ink); }
      d.fillCircle(168, 58, 6, pap); d.drawCircle(168, 58, 6, ink); d.drawCircle(168, 58, 7, ink);
      break;
    }
    case 7: {   // night sky, a dazzling new star, Venus and a crescent
      sky(d, 7, 36, 22, 90);
      d.fillCircle(146, 44, 4, ink); rays(d, 146, 44, 6, 17, 8, ink); rays(d, 146, 44, 6, 11, 16, ink);
      crescent(d, 40, 40, 11, 5);
      d.drawCircle(96, 70, 3, ink); d.fillCircle(96, 70, 1, ink);
      d.drawFastHLine(0, 94, 200, ink);
      break;
    }
    case 8: {   // Andromeda: a faint tilted ellipse
      sky(d, 11, 20, 22, 94);
      uint32_t h = 12345u;
      for (int i = 0; i < 260; i++) {
        h = h * 1103515245u + 12345u; float u = ((h >> 8) & 1023) / 1023.0f;
        h = h * 1103515245u + 12345u; float w = ((h >> 8) & 1023) / 1023.0f - 0.5f;
        float a = (u - 0.5f) * 2.0f; float rr = fabsf(a); if ((h >> 4) % 100 > (int)(100 - 60 * rr)) continue;
        float ex = a * 66.0f, ey = w * 22.0f * (1.0f - rr * 0.6f), ca = -0.35f;
        d.drawPixel(100 + (int)lroundf(ex * cosf(ca) - ey * sinf(ca)), 58 + (int)lroundf(ex * sinf(ca) + ey * cosf(ca)), ink);
      }
      d.fillCircle(100, 58, 2, ink); d.drawCircle(100, 58, 5, ink);
      break;
    }
    case 9: {   // Pleiades and Aldebaran
      sky(d, 3, 24, 22, 94);
      int px[7] = {34, 41, 47, 38, 45, 52, 43}, py[7] = {40, 36, 42, 47, 51, 46, 58};
      for (int k = 0; k < 7; k++) { d.fillCircle(px[k], py[k], 2, ink); }
      d.fillCircle(140, 56, 4, ink); rays(d, 140, 56, 6, 11, 8, ink);
      d.fillTriangle(110, 44, 124, 52, 120, 58, ink);
      for (int x = 60; x < 128; x += 5) d.drawPixel(x, 48 + (x - 60) / 10, ink);
      arrow(d, 100, 49, 0.25f, 14, 4);
      break;
    }
    case 10: {   // Tusi couple
      int cx = 100, cy = 58, R = 36; float th = 0.7f;
      d.drawCircle(cx, cy, R, ink);
      int sx = cx + (int)lroundf(R / 2 * cosf(th)), sy = cy - (int)lroundf(R / 2 * sinf(th));
      d.drawCircle(sx, sy, R / 2, ink);
      int px = cx + (int)lroundf(R * cosf(th)); d.drawFastHLine(cx - R - 6, cy, 2 * R + 12, ink);
      d.fillCircle(px, cy, 3, ink); d.drawLine(sx, sy, px, cy, ink); d.fillCircle(sx, sy, 1, ink);
      break;
    }
    case 11: {   // year ring, calendar marker drifting from the sun
      int cx = 100, cy = 58, R = 32;
      d.drawCircle(cx, cy, R, ink);
      for (int k = 0; k < 12; k++) { float a = k * 0.5236f; d.drawLine(cx + (int)lroundf((R - 4) * sinf(a)), cy - (int)lroundf((R - 4) * cosf(a)), cx + (int)lroundf(R * sinf(a)), cy - (int)lroundf(R * cosf(a)), ink); }
      d.fillCircle(cx, cy - R - 8, 5, ink); rays(d, cx, cy - R - 8, 7, 10, 8, ink);
      float a = v * 0.0524f; int fx = cx + (int)lroundf(R * sinf(a)), fy = cy - (int)lroundf(R * cosf(a));
      d.drawLine(cx, cy, fx, fy, ink); d.drawLine(fx, fy, fx, fy - 10, ink); d.fillTriangle(fx, fy - 10, fx + 8, fy - 7, fx, fy - 4, ink);
      break;
    }
    case 12: {   // giant sextant arc with a tiny person
      int cx = 20, cy = 98;
      for (int a = 0; a <= 60; a += 1) { float r = a * 0.017453f; d.drawPixel(cx + (int)lroundf(150 * cosf(r)), cy - (int)lroundf(150 * sinf(r)), ink); d.drawPixel(cx + (int)lroundf(149 * cosf(r)), cy - (int)lroundf(149 * sinf(r)), ink); }
      for (int a = 0; a <= 60; a += 2) { float r = a * 0.017453f; int l = a % 10 == 0 ? 8 : 4; d.drawLine(cx + (int)lroundf((150 - l) * cosf(r)), cy - (int)lroundf((150 - l) * sinf(r)), cx + (int)lroundf(150 * cosf(r)), cy - (int)lroundf(150 * sinf(r)), ink); }
      d.drawLine(cx, cy, cx + 150, cy, ink); d.drawLine(cx, cy, cx + 75, cy - 130, ink);
      d.drawFastHLine(0, 97, 200, ink);
      d.fillCircle(96, 84, 2, ink); d.drawLine(96, 86, 96, 93, ink); d.drawLine(96, 93, 94, 97, ink); d.drawLine(96, 93, 98, 97, ink); d.drawLine(93, 89, 99, 89, ink);
      d.drawFastHLine(110, 60, 0, ink);
      break;
    }
    case 14: {   // dark room: candle, pinhole, upside-down image
      d.drawFastVLine(90, 24, 34, ink); d.drawFastVLine(90, 63, 31, ink);
      d.drawFastHLine(90, 24, 100, ink); d.drawFastHLine(90, 93, 100, ink); d.drawFastVLine(190, 24, 70, ink);
      d.drawFastVLine(30, 52, 24, ink); d.drawFastVLine(31, 52, 24, ink); d.fillCircle(30, 47, 3, ink);
      d.drawLine(30, 47, 170, 77, ink); d.drawLine(30, 76, 170, 39, ink);
      d.drawFastVLine(170, 39, 34, ink); d.drawFastVLine(171, 39, 34, ink); d.fillCircle(170, 77, 3, ink);
      break;
    }
    case 15: {   // al-Khwarizmi: completing the square
      int x0 = 70, y0 = 28, sd = 38, w = 16;
      d.drawRect(x0, y0, sd, sd, ink); d.drawRect(x0 + sd, y0, w, sd, ink); d.drawRect(x0, y0 + sd, sd, w, ink); d.drawRect(x0 + sd, y0 + sd, w, w, ink);
      for (int y = y0 + sd + 2; y < y0 + sd + w - 1; y += 3) for (int x = x0 + sd + 2; x < x0 + sd + w - 1; x += 3) d.drawPixel(x, y, ink);
      d.setFont(nullptr); d.setTextSize(1);
      d.setCursor(x0 + 16, y0 + 15); d.print("x*x"); d.setCursor(x0 + sd + 5, y0 + 15); d.print("5x"); d.setCursor(x0 + 14, y0 + sd + 4); d.print("5x");
      d.setCursor(x0 + sd + 2, y0 + sd + 4); d.print("25");
      d.setFont(&NotoMono13);
      break;
    }
    case 16: {   // cipher wheel: the inner ring is shifted by v steps
      int cx = 100, cy = 58;
      d.drawCircle(cx, cy, 34, ink); d.drawCircle(cx, cy, 25, ink); d.drawCircle(cx, cy, 24, ink); d.drawCircle(cx, cy, 14, ink);
      for (int i = 0; i < 26; i++) {
        float a = i * 0.24166f, b = (i + v) * 0.24166f;
        d.drawLine(cx + (int)lroundf(25 * sinf(a)), cy - (int)lroundf(25 * cosf(a)), cx + (int)lroundf(34 * sinf(a)), cy - (int)lroundf(34 * cosf(a)), ink);
        d.drawLine(cx + (int)lroundf(14 * sinf(b)), cy - (int)lroundf(14 * cosf(b)), cx + (int)lroundf(24 * sinf(b)), cy - (int)lroundf(24 * cosf(b)), ink);
      }
      float ae = 4 * 0.24166f, be = (4 + v) * 0.24166f;
      d.fillCircle(cx + (int)lroundf(29.5f * sinf(ae)), cy - (int)lroundf(29.5f * cosf(ae)), 3, ink);
      d.fillCircle(cx + (int)lroundf(19 * sinf(be)), cy - (int)lroundf(19 * cosf(be)), 3, ink);
      break;
    }
    case 17: {   // map with the compass arrow pointing down
      d.drawRect(24, 26, 140, 64, ink); d.drawRect(26, 28, 136, 60, ink);
      int px[7] = {40, 60, 80, 100, 120, 140, 155}, py[7] = {62, 48, 54, 42, 58, 50, 66};
      for (int k = 0; k < 6; k++) { d.drawLine(px[k], py[k], px[k + 1], py[k + 1], ink); d.drawLine(px[k], py[k] + 1, px[k + 1], py[k + 1] + 1, ink); }
      for (int x = 34; x < 156; x += 7) d.drawPixel(x, 80 + (x / 7) % 3, ink);
      arrow(d, 184, 34, 1.5708f, 38, 5);
      d.setFont(nullptr); d.setTextSize(1); d.setCursor(181, 76); d.print("N"); d.setFont(&NotoMono13);
      break;
    }
    case 18: {   // heart between two lungs
      for (int k = 0; k < 2; k++) {
        int cx = k ? 150 : 50;
        for (int a = 0; a < 360; a += 2) { float r = a * 0.017453f; d.drawPixel(cx + (int)lroundf(22 * cosf(r)), 58 + (int)lroundf(28 * sinf(r)), ink); d.drawPixel(cx + (int)lroundf(21 * cosf(r)), 58 + (int)lroundf(27 * sinf(r)), ink); }
        d.drawLine(cx, 34, cx + (k ? -8 : 8), 52, ink);
      }
      d.fillCircle(92, 52, 9, ink); d.fillCircle(108, 52, 9, ink); d.fillTriangle(84, 57, 116, 57, 100, 80, ink);
      arrow(d, 82, 50, 3.14159f, 14, 4); arrow(d, 118, 50, 0, 14, 4);
      arrow(d, 72, 66, 0.0f, 10, 4); arrow(d, 128, 66, 3.14159f, 10, 4);
      break;
    }
    case 19: {   // crank wheel, rod and piston
      int cx = 56, cy = 60, px = cx + 17, py = cy - 17;
      d.drawCircle(cx, cy, 26, ink); d.drawCircle(cx, cy, 25, ink); d.fillCircle(cx, cy, 3, ink);
      d.drawLine(cx, cy, px, py, ink); d.drawLine(cx + 1, cy, px + 1, py, ink); d.fillCircle(px, py, 3, ink);
      d.drawLine(px, py, 128, 60, ink); d.drawLine(px, py + 1, 128, 61, ink);
      d.fillRect(128, 50, 20, 20, ink); d.drawRect(122, 44, 70, 32, ink);
      arrow(d, 160, 87, 0.0f, 22, 4); arrow(d, 190, 87, 3.14159f, 22, 4);
      break;
    }
    case 20: {   // circle with an inscribed 12-gon
      int cx = 100, cy = 58, R = 34;
      d.drawCircle(cx, cy, R, ink);
      for (int k = 0; k < 12; k++) {
        float a = k * 0.5236f, b = (k + 1) * 0.5236f;
        d.drawLine(cx + (int)lroundf(R * cosf(a)), cy + (int)lroundf(R * sinf(a)), cx + (int)lroundf(R * cosf(b)), cy + (int)lroundf(R * sinf(b)), ink);
        d.drawLine(cx + (int)lroundf(R * cosf(a)) + 1, cy + (int)lroundf(R * sinf(a)), cx + (int)lroundf(R * cosf(b)) + 1, cy + (int)lroundf(R * sinf(b)), ink);
      }
      break;
    }
    case 21: {   // raindrop: ray enters, reflects inside, leaves
      int cx = 110, cy = 58, R = 26;
      d.fillCircle(14, 40, 4, ink); rays(d, 14, 40, 6, 10, 8, ink);
      d.drawCircle(cx, cy, R, ink);
      float yi = 46; float xi = cx - sqrtf((float)(R * R) - (yi - cy) * (yi - cy));
      float ab = 0.70f, ae = 2.2f;
      int bx = cx + (int)lroundf(R * cosf(ab)), by = cy + (int)lroundf(R * sinf(ab));
      int ex = cx + (int)lroundf(R * cosf(ae)), ey = cy + (int)lroundf(R * sinf(ae));
      d.drawLine(20, 46, (int)xi, 46, ink); d.drawLine((int)xi, 46, bx, by, ink); d.drawLine(bx, by, ex, ey, ink); d.drawLine(ex, ey, 40, 96, ink); d.drawLine(ex, ey + 1, 41, 97, ink);
      break;
    }
    case 22: {   // two friends: overlapping circles
      d.drawCircle(78, 58, 30, ink); d.drawCircle(78, 58, 29, ink); d.drawCircle(122, 58, 30, ink); d.drawCircle(122, 58, 29, ink);
      d.setFont(nullptr); d.setTextSize(2); d.setCursor(42, 51); d.print("220"); d.setCursor(112, 51); d.print("284"); d.setTextSize(1); d.setFont(&NotoMono13);
      break;
    }
    case 23: {   // ship with winds blowing both ways
      for (int x = 4; x < 196; x++) d.drawPixel(x, 78 + (int)lroundf(2.0f * sinf(x * 0.35f)), ink);
      d.fillTriangle(70, 70, 130, 70, 120, 78, ink); d.fillTriangle(70, 70, 56, 62, 70, 62, ink);
      d.drawFastVLine(100, 34, 36, ink); d.fillTriangle(100, 34, 100, 64, 126, 64, ink); d.fillTriangle(98, 40, 98, 64, 78, 64, ink);
      d.fillCircle(24, 30, 5, ink); rays(d, 24, 30, 7, 11, 8, ink); arrow(d, 40, 26, 0.0f, 40, 6);
      for (int k = 0; k < 3; k++) d.drawPixel(160 + k * 8, 28 + k, ink);
      arrow(d, 190, 36, 3.14159f, 40, 6);
      break;
    }
    case 24: {   // eight-pointed star in a circle (two squares)
      int cx = 100, cy = 58, R = 32;
      d.drawCircle(cx, cy, R + 3, ink);
      for (int k = 0; k < 2; k++) {
        float o = k ? 0.7854f : 0.0f;
        for (int i = 0; i < 4; i++) {
          float a = o + i * 1.5708f + 0.7854f * 0.0f, b = a + 1.5708f;
          d.drawLine(cx + (int)lroundf(R * 1.0f * cosf(a + 0.7854f)), cy + (int)lroundf(R * sinf(a + 0.7854f)), cx + (int)lroundf(R * cosf(b + 0.7854f)), cy + (int)lroundf(R * sinf(b + 0.7854f)), ink);
        }
      }
      d.drawCircle(cx, cy, 9, ink); d.fillCircle(cx, cy, 3, ink);
      break;
    }
    case 25: {   // stack of sheets with a folded corner and lines of writing
      d.drawRect(84, 28, 56, 64, ink); d.drawRect(78, 23, 56, 64, ink);
      d.fillRect(70, 18, 56, 64, pap); d.drawRect(70, 18, 56, 64, ink);
      d.fillTriangle(110, 18, 126, 34, 110, 34, ink);
      for (int k = 0; k < 5; k++) d.drawFastHLine(78, 42 + k * 8, k == 4 ? 20 : 40, ink);
      break;
    }
    case 26: {   // a ring (zero) and a dot
      d.fillCircle(80, 58, 28, ink); d.fillCircle(80, 58, 21, pap);
      d.fillCircle(140, 58, 5, ink);
      d.drawFastHLine(8, 90, 184, ink);
      break;
    }
    case 27: {   // Pascal's triangle
      static const char *const R[6] = {"1", "1 1", "1 2 1", "1 3 3 1", "1 4 6 4 1", "1 5 ? ? 5 1"};
      d.setFont(nullptr); d.setTextSize(1);
      for (int k = 0; k < 6; k++) { int w = (int)strlen(R[k]) * 6 - 1; d.setCursor(100 - w / 2, 22 + k * 12); d.print(R[k]); }
      d.setFont(&NotoMono13);
      break;
    }
    case 28: {   // hospital: dome, arcades and a fountain court
      d.drawRect(30, 46, 140, 44, ink); d.drawRect(31, 47, 138, 42, ink);
      d.fillCircle(100, 34, 14, ink); d.fillRect(80, 34, 40, 14, pap); d.drawRect(86, 30, 28, 18, ink);
      d.drawFastVLine(100, 14, 12, ink); d.fillCircle(100, 12, 3, ink);
      for (int k = 0; k < 4; k++) { int x = 44 + k * 28; d.drawRect(x, 62, 16, 28, ink); d.fillCircle(x + 8, 62, 8, ink); d.fillRect(x + 3, 64, 10, 26, pap); }
      break;
    }
    case 29: {   // two skins with different spots
      d.drawCircle(56, 56, 34, ink); d.drawCircle(144, 56, 34, ink);
      static const int8_t A[6][2] = {{-14, -16}, {12, -8}, {-4, 6}, {16, 18}, {-20, 12}, {2, -26}};
      for (int k = 0; k < 6; k++) { d.fillCircle(56 + A[k][0], 56 + A[k][1], 4, ink); }
      for (int i = 0; i < 16; i++) { int x = 144 + ((i * 7) % 11 - 5) * 5, y = 56 + ((i * 5) % 9 - 4) * 6; if ((x - 144) * (x - 144) + (y - 56) * (y - 56) < 28 * 28) d.fillRect(x, y, 2, 2, ink); }
      break;
    }
    case 30: {   // scalpel and forceps
      d.drawLine(30, 80, 100, 30, ink); d.drawLine(31, 80, 101, 30, ink); d.drawLine(30, 81, 100, 31, ink);
      d.fillTriangle(100, 30, 118, 18, 104, 38, ink);
      d.drawLine(120, 86, 168, 30, ink); d.drawLine(120, 30, 168, 86, ink);
      d.drawLine(121, 86, 169, 30, ink); d.drawLine(121, 30, 169, 86, ink);
      d.drawCircle(122, 26, 5, ink); d.drawCircle(166, 26, 5, ink);
      break;
    }
    default: {  // title: dome, crescent and stars
      sky(d, 5, 40, 20, 62);
      crescent(d, 160, 38, 11, 5);
      for (int a = 180; a <= 360; a++) { float r = a * 0.017453f; d.drawPixel(100 + (int)lroundf(30 * cosf(r)), 92 + (int)lroundf(30 * sinf(r)), ink); d.drawPixel(100 + (int)lroundf(29 * cosf(r)), 92 + (int)lroundf(29 * sinf(r)), ink); }
      d.drawFastHLine(60, 92, 80, ink); d.fillRect(98, 62, 5, 30, pap);
      d.drawLine(98, 62, 98, 92, ink); d.drawLine(102, 62, 102, 92, ink);
      d.drawFastHLine(0, 93, 200, ink);
      break;
    }
  }
}
// letters under drawing 2 (stick names)
inline void stickLetters(Adafruit_GFX &d) {
  setup13(d);
  const char *L = "ABCD";
  for (int k = 0; k < 4; k++) { char c[2] = {L[k], 0}; d.setCursor(10 + k * 47 + 30, 101); d.print(c); }
}

// ---------------------------------------------------------------- screens
inline void drawMenu(Adafruit_GFX &d) {
  char t[12]; snprintf(t, sizeof(t), "%d/%d", total(), NM * 3);
  drawDia(d, 13, 0);
  setup13(d);
  txtN(d, "THE OBSERVATORY", 15, 6, 12); txtN(d, t, (int)strlen(t), 194 - 8 * (int)strlen(t), 12); d.drawFastHLine(4, 15, 192, qwInk());
  const char *nm[3] = {"MISSIONS", "CODEX", "ABOUT"};
  for (int i = 0; i < 3; i++) {
    int y = 104 + i * 25;
    if (i == sel) d.fillRect(20, y, 160, 22, qwInk());
    d.setTextColor(i == sel ? qwPaper() : qwInk());
    txtN(d, nm[i], (int)strlen(nm[i]), 100 - 4 * (int)strlen(nm[i]), y + 16);
    d.setTextColor(qwInk());
  }
}
inline void stars3(char *o, int n) { o[0] = n >= 1 ? '*' : '.'; o[1] = n >= 2 ? '*' : '.'; o[2] = n >= 3 ? '*' : '.'; o[3] = 0; }
inline void drawList(Adafruit_GFX &d, bool codex) {
  char t[12]; snprintf(t, sizeof(t), "%d/%d", total(), NM * 3);
  header(d, codex ? "CODEX" : "MISSIONS", t);
  int first = sel > 4 ? sel - 4 : 0; if (first > NM - 9) first = NM - 9; if (first < 0) first = 0;
  for (int k = 0; k < 9 && first + k < NM; k++) {
    int i = first + k, y = 19 + k * 20;
    char b[32], s[4]; stars3(s, st[i]);
    bool open = codex ? st[i] > 0 : unlocked(i);
    if (open) snprintf(b, sizeof(b), "%2d %-13.13s %s", i + 1, M[i].name, codex ? "" : s); else snprintf(b, sizeof(b), "%2d %-13.13s %s", i + 1, codex ? "..." : M[i].name, codex ? "" : "lock");
    if (i == sel) d.fillRect(4, y, 192, 19, qwInk());
    setup13(d); d.setTextColor(i == sel ? qwPaper() : qwInk());
    txtN(d, b, (int)strlen(b), 8, y + 14);
    d.setTextColor(qwInk());
  }
}
// story / fact / about / end pages
inline void drawText(Adafruit_GFX &d, const char *title, const char *right, int dia, const char *last) {
  bool hasDia = dia >= 0;
  wrap(buf);
  int np = nPages(hasDia); if (page >= np) page = (uint8_t)(np - 1);
  int a, n; pageRange(hasDia, page, a, n);
  header(d, title, right);
  int y0;
  if (hasDia && page == 0) { drawDia(d, dia, 0); if (dia == 2) stickLetters(d); y0 = 112; } else { y0 = 32; if (!hasDia) y0 = 32; }
  setup13(d);
  for (int k = 0; k < n; k++) txtN(d, buf + ls[a + k], ll[a + k], 6, y0 + k * 15);
  char pg[8]; snprintf(pg, sizeof(pg), "%d/%d", page + 1, np);
  footer(d, page < np - 1 ? "MENU: next" : last, np > 1 ? pg : nullptr);
}
inline void drawAsk(Adafruit_GFX &d) {
  const Mis &m = M[cur];
  char r[8]; snprintf(r, sizeof(r), "TRY %d", tries + 1);
  header(d, m.name, tries ? r : nullptr);
  setup13(d);
  if (m.kind == 1) {
    drawDia(d, m.dia, dv);
    snprintf(buf, sizeof(buf), "%s", tries ? m.hint : m.q);
    wrap(buf);
    int n = nl < 3 ? nl : 3;
    for (int k = 0; k < n; k++) txtN(d, buf + ls[k], ll[k], 6, 112 + k * 15);
    char v[24]; snprintf(v, sizeof(v), "%d %s", dv, m.opt[0]);
    d.fillRect(40, 157, 120, 22, qwInk()); d.setTextColor(qwPaper()); txtN(d, v, (int)strlen(v), 100 - 4 * (int)strlen(v), 173); d.setTextColor(qwInk());
    footer(d, "UP/DOWN set  MENU: ok", nullptr);
    return;
  }
  int y = 20;
  if (m.kind == 2) { drawDia(d, m.dia, 0); if (m.dia == 2) stickLetters(d); y = 114; }
  snprintf(buf, sizeof(buf), "%s", tries ? m.hint : m.q);
  wrap(buf);
  int maxl = m.kind == 2 ? 1 : 6; int n = nl < maxl ? nl : maxl;
  for (int k = 0; k < n; k++) txtN(d, buf + ls[k], ll[k], 6, y + 12 + k * 15);
  int oy = m.kind == 2 ? 128 : 128;
  for (int i = 0; i < 4; i++) {
    int yy = oy + i * 17;
    bool w = wrong >> i & 1;
    if (i == sel) d.fillRect(4, yy, 192, 17, qwInk());
    d.setTextColor(i == sel ? qwPaper() : qwInk());
    char b[32]; snprintf(b, sizeof(b), "%s%.22s", w ? "x " : "", m.opt[i]);
    txtN(d, b, (int)strlen(b), 8, yy + 13);
    d.setTextColor(qwInk());
  }
}

// ---------------------------------------------------------------- logic
inline void buildFor() {
  const Mis &m = M[cur];
  switch (phase) {
    case PH_STORY: snprintf(buf, sizeof(buf), "%s", m.story); break;
    case PH_FACT: {
      char hd[64];
      if (fromCodex) hd[0] = 0;
      else if (tries == 0) snprintf(hd, sizeof(hd), "PERFECT! ***\n");
      else if (tries < 3) snprintf(hd, sizeof(hd), "WELL DONE! %s\n", tries == 1 ? "**." : "*..");
      else if (m.kind == 1) snprintf(hd, sizeof(hd), "ANSWER: %d %s\n", (int)m.right, m.opt[0]);
      else snprintf(hd, sizeof(hd), "ANSWER: %s\n", m.opt[m.right]);
      snprintf(buf, sizeof(buf), "%s%s", hd, m.fact);
      break;
    }
    case PH_ABOUT: snprintf(buf, sizeof(buf), "%s", ABOUT); break;
    case PH_END: { char t[24]; snprintf(t, sizeof(t), "\n\nStars: %d / %d", total(), NM * 3); snprintf(buf, sizeof(buf), "%s%s", FINALE, t); break; }
    default: buf[0] = 0;
  }
}
inline void startMission(int i) { cur = (uint8_t)i; phase = PH_STORY; page = 0; tries = 0; wrong = 0; sel = 0; dv = M[i].dstart; fromCodex = 0; }
inline void finish() {
  int s = tries == 0 ? 3 : (tries == 1 ? 2 : 1);
  if (s > st[cur]) st[cur] = (uint8_t)s;
  save();
  phase = PH_FACT; page = 0;
}
inline void answer() {
  const Mis &m = M[cur];
  bool ok;
  if (m.kind == 1) { int e = dv - m.right; ok = (e < 0 ? -e : e) <= m.tol; }
  else { if (wrong >> sel & 1) return; ok = (sel == m.right); }
  if (ok) { finish(); return; }
  tries++;
  if (m.kind != 1) wrong |= (uint8_t)(1 << sel);
  if (tries >= 3) { tries = 3; finish(); tries = 3; }
  else if (m.kind != 1) { for (int i = 0; i < 4; i++) if (!(wrong >> i & 1)) { sel = (uint8_t)i; break; } }
}

inline void start() { load(); phase = PH_MENU; sel = 0; page = 0; }
inline bool busy() { return false; }
inline bool back() {
  load();
  switch (phase) {
    case PH_STORY: case PH_ASK: phase = PH_LIST; sel = cur; return true;
    case PH_FACT: if (fromCodex) { phase = PH_CODEX; sel = cur; } else { phase = PH_LIST; sel = cur; } return true;
    case PH_LIST: phase = PH_MENU; sel = 0; return true;
    case PH_CODEX: phase = PH_MENU; sel = 1; return true;
    case PH_ABOUT: phase = PH_MENU; sel = 2; return true;
    case PH_END: phase = PH_LIST; sel = NM - 1; return true;
    default: return false;
  }
}
inline void moveSel(int dir, int n) { int s = qwGameSteps(); s = s == 1 ? 1 : (s == 3 ? 3 : 6); sel = (uint8_t)(((int)sel + dir * s + n * 8) % n); }
inline void up() {
  load();
  switch (phase) {
    case PH_MENU: moveSel(-1, 3); break;
    case PH_LIST: case PH_CODEX: moveSel(-1, NM); break;
    case PH_STORY: case PH_FACT: case PH_ABOUT: case PH_END: if (page > 0) page--; break;
    case PH_ASK:
      if (M[cur].kind == 1) {
        int s = qwGameSteps(); int stp = (M[cur].dmax - M[cur].dmin > 100) ? (s == 1 ? 1 : (s == 3 ? 5 : 15)) : 1;
        dv += stp; if (dv > M[cur].dmax) dv = (M[cur].dmax - M[cur].dmin > 100) ? M[cur].dmin : M[cur].dmax;
      } else sel = (uint8_t)((sel + 3) % 4);
      break;
  }
}
inline void down() {
  load();
  switch (phase) {
    case PH_MENU: moveSel(1, 3); break;
    case PH_LIST: case PH_CODEX: moveSel(1, NM); break;
    case PH_STORY: case PH_FACT: case PH_ABOUT: case PH_END: page++; break;   // clamped when drawn
    case PH_ASK:
      if (M[cur].kind == 1) {
        int s = qwGameSteps(); int stp = (M[cur].dmax - M[cur].dmin > 100) ? (s == 1 ? 1 : (s == 3 ? 5 : 15)) : 1;
        dv -= stp; if (dv < M[cur].dmin) dv = (M[cur].dmax - M[cur].dmin > 100) ? M[cur].dmax : M[cur].dmin;
      } else sel = (uint8_t)((sel + 1) % 4);
      break;
  }
}
inline void select() {
  load();
  switch (phase) {
    case PH_MENU: if (sel == 0) { phase = PH_LIST; sel = 0; for (int i = 0; i < NM; i++) if (unlocked(i) && !st[i]) { sel = (uint8_t)i; break; } } else if (sel == 1) { phase = PH_CODEX; sel = 0; } else { phase = PH_ABOUT; page = 0; } break;
    case PH_LIST: if (unlocked(sel)) startMission(sel); break;
    case PH_CODEX: if (st[sel]) { cur = sel; fromCodex = 1; phase = PH_FACT; page = 0; } break;
    case PH_STORY: case PH_FACT: case PH_ABOUT: case PH_END: {
      buildFor(); wrap(buf);
      bool hasDia = phase == PH_STORY || phase == PH_END;
      int np = nPages(hasDia);
      if (page < np - 1) { page++; break; }
      if (phase == PH_STORY) { phase = PH_ASK; sel = 0; page = 0; }
      else if (phase == PH_ABOUT) { phase = PH_MENU; sel = 2; }
      else if (phase == PH_END) { phase = PH_LIST; sel = NM - 1; }
      else if (fromCodex) { phase = PH_CODEX; sel = cur; }
      else if (cur == NM - 1 && allDone()) { phase = PH_END; page = 0; }
      else { phase = PH_LIST; sel = (uint8_t)(cur + 1 < NM ? cur + 1 : cur); }
      break;
    }
    case PH_ASK: answer(); break;
  }
}
inline void draw(Adafruit_GFX &d) {
  load();
  d.fillScreen(qwPaper());
  buildFor();
  switch (phase) {
    case PH_MENU: drawMenu(d); break;
    case PH_LIST: drawList(d, false); break;
    case PH_CODEX: drawList(d, true); break;
    case PH_STORY: { char r[8]; snprintf(r, sizeof(r), "%d/%d", cur + 1, NM); drawText(d, M[cur].name, r, M[cur].dia, "MENU: question"); break; }
    case PH_FACT: drawText(d, M[cur].name, fromCodex ? "CODEX" : "FACT", -1, fromCodex ? "MENU: back" : "MENU: continue"); break;
    case PH_ABOUT: drawText(d, "ABOUT", nullptr, -1, "MENU: back"); break;
    case PH_END: drawText(d, "WELL DONE", nullptr, 13, "MENU: finish"); break;
    case PH_ASK: drawAsk(d); break;
  }
  d.setFont(nullptr);
}

}  // namespace Ob
