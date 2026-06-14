// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Core Wallet Implementation

#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "wallet/crypter.h"
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/ecdsa.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <openssl/hmac.h>
#include <openssl/obj_mac.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <random>
#include <ctime>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#endif

namespace bht {
namespace wallet {

constexpr int BIP39_WORDLIST_SIZE = 2299;

const char* GetBIP39Word(int index) {
    static const char* BIP39_WORDLIST[2299] = {
        "abandon", "ability", "able", "about", "above", "absent", "absorb", "abstract",
        "absurd", "abuse", "access", "accident", "account", "accuse", "achieve", "acid",
        "acoustic", "acquire", "across", "act", "action", "actor", "actress", "actual",
        "adapt", "add", "addict", "address", "adjust", "admit", "adult", "advance",
        "advice", "aerobic", "affair", "afford", "afraid", "again", "age", "agent",
        "agree", "ahead", "aim", "air", "airport", "aisle", "alarm", "album",
        "alcohol", "alert", "alien", "all", "alley", "allow", "almost", "alone",
        "alpha", "already", "also", "alter", "always", "amateur", "amazing", "among",
        "amount", "amused", "analyst", "anchor", "ancient", "anger", "angle", "angry",
        "animal", "ankle", "announce", "annual", "another", "answer", "antenna", "antique",
        "anxiety", "any", "apart", "apology", "appear", "apple", "approve", "april",
        "arch", "arctic", "area", "arena", "argue", "arm", "armed", "armor",
        "army", "around", "arrange", "arrest", "arrive", "arrow", "art", "artefact",
        "artist", "artwork", "ask", "aspect", "assault", "asset", "assist", "assume",
        "asthma", "athlete", "atom", "attack", "attend", "attitude", "attract", "auction",
        "audit", "august", "aunt", "author", "auto", "autumn", "average", "avocado",
        "avoid", "awake", "aware", "away", "awesome", "awful", "awkward", "axis",
        "baby", "bachelor", "bacon", "badge", "bag", "balance", "balcony", "ball",
        "bamboo", "banana", "banner", "bar", "barely", "bargain", "barrel", "base",
        "basic", "basket", "battle", "beach", "bean", "beauty", "because", "become",
        "beef", "before", "begin", "behave", "behind", "believe", "below", "belt",
        "bench", "benefit", "best", "betray", "better", "between", "beyond", "bicycle",
        "bid", "bike", "bind", "biology", "bird", "birth", "bitter", "black",
        "blade", "blame", "blanket", "blast", "bleak", "bless", "blind", "blood",
        "blossom", "blouse", "blue", "blur", "blush", "board", "boat", "body",
        "boil", "bomb", "bone", "bonus", "book", "boost", "border", "boring",
        "borrow", "boss", "bottom", "bounce", "box", "boy", "bracket", "brain",
        "brand", "brass", "brave", "bread", "breeze", "brick", "bridge", "brief",
        "bright", "bring", "brisk", "broccoli", "broken", "bronze", "broom", "brother",
        "brown", "brush", "bubble", "buddy", "budget", "buffalo", "build", "bulb",
        "bulk", "bullet", "bundle", "bamboo", "burden", "burger", "burst", "bus",
        "business", "busy", "butter", "buyer", "buzz", "cabbage", "cabin", "cable",
        "cactus", "cage", "cake", "call", "calm", "camera", "camp", "can",
        "canal", "cancel", "candy", "cannon", "canoe", "canvas", "canyon", "capable",
        "capital", "captain", "car", "carbon", "card", "cargo", "carpet", "carry",
        "cart", "case", "cash", "casino", "castle", "casual", "cat", "catalog",
        "catch", "category", "cattle", "caught", "cause", "caution", "cave", "ceiling",
        "celery", "cement", "census", "century", "cereal", "certain", "chair", "chalk",
        "champion", "change", "chaos", "chapter", "charge", "chase", "chat", "cheap",
        "check", "cheese", "chef", "cherry", "chest", "chicken", "chief", "child",
        "chimney", "choice", "choose", "chronic", "chuckle", "chunk", "churn", "cigar",
        "cinnamon", "circle", "citizen", "city", "civil", "claim", "clap", "clarify",
        "claw", "clay", "clean", "clerk", "clever", "click", "client", "cliff",
        "climb", "clinic", "clip", "clock", "clog", "close", "cloth", "cloud",
        "clown", "club", "clump", "cluster", "clutch", "coach", "coast", "coconut",
        "code", "coffee", "coil", "coin", "collect", "color", "comet", "comfort",
        "comic", "common", "company", "concert", "conduct", "confirm", "congress", "connect",
        "consider", "control", "convince", "cook", "cool", "copper", "copy", "coral",
        "core", "corn", "correct", "cost", "cotton", "couch", "country", "couple",
        "course", "cousin", "cover", "coyote", "crack", "cradle", "craft", "cram",
        "crane", "crash", "crater", "crawl", "crazy", "cream", "credit", "creek",
        "crew", "cricket", "crime", "crisp", "critic", "crop", "cross", "crouch",
        "crowd", "crucial", "cruel", "cruise", "crumble", "crunch", "crush", "cry",
        "crystal", "cube", "culture", "cup", "cupboard", "curious", "current", "curtain",
        "curve", "cushion", "custom", "cute", "cycle", "dad", "damage", "damp",
        "dance", "danger", "daring", "dash", "daughter", "dawn", "day", "deal",
        "debate", "debris", "decade", "december", "decide", "decline", "decorate", "decrease",
        "deer", "defense", "define", "defy", "degree", "delay", "deliver", "demand",
        "demise", "denial", "dentist", "deny", "depart", "depend", "deposit", "depth",
        "deputy", "derive", "describe", "desert", "design", "desk", "despair", "destroy",
        "detail", "detect", "develop", "device", "devote", "diagram", "dial", "diamond",
        "diary", "dice", "diesel", "diet", "differ", "digital", "dignity", "dilemma",
        "dinner", "dinosaur", "direct", "dirt", "disagree", "discover", "disease", "dish",
        "dismiss", "disorder", "display", "distance", "divert", "divide", "divorce", "dizzy",
        "doctor", "document", "dog", "doll", "dolphin", "domain", "donate", "donkey",
        "donor", "door", "dose", "double", "dove", "draft", "dragon", "drama",
        "drastic", "draw", "dream", "dress", "drift", "drill", "drink", "drip",
        "drive", "drop", "drum", "dry", "duck", "dumb", "dune", "during",
        "dust", "dutch", "duty", "dwarf", "dynamic", "eager", "eagle", "early",
        "earn", "earth", "easily", "east", "easy", "echo", "ecology", "economy",
        "edge", "edit", "educate", "effort", "egg", "eight", "either", "elbow",
        "elder", "electric", "elegant", "element", "elephant", "elevator", "elite", "else",
        "embark", "embody", "embrace", "emerge", "emotion", "employ", "empower", "empty",
        "enable", "enact", "end", "endless", "endorse", "enemy", "energy", "enforce",
        "engage", "engine", "enhance", "enjoy", "enlist", "enough", "enrich", "enroll",
        "ensure", "enter", "entire", "entry", "envelope", "episode", "equal", "equip",
        "era", "erase", "erode", "erosion", "error", "erupt", "escape", "essay",
        "essence", "estate", "eternal", "ethics", "evidence", "evil", "evoke", "evolve",
        "exact", "example", "excess", "exchange", "excite", "exclude", "excuse", "execute",
        "exercise", "exhaust", "exhibit", "exile", "exist", "exit", "exotic", "expand",
        "expect", "expire", "explain", "expose", "express", "extend", "extra", "eye",
        "eyebrow", "fabric", "face", "faculty", "fade", "faint", "faith", "fall",
        "false", "fame", "family", "famous", "fan", "fancy", "fantasy", "farm",
        "fashion", "fat", "fatal", "father", "fatigue", "fault", "favorite", "feature",
        "february", "federal", "fee", "feed", "feel", "female", "fence", "festival",
        "fetch", "fever", "few", "fiber", "fiction", "field", "figure", "file",
        "film", "filter", "final", "find", "fine", "finger", "finish", "fire",
        "firm", "first", "fiscal", "fish", "fit", "fitness", "fix", "flag",
        "flame", "flash", "flat", "flavor", "flee", "flight", "flip", "float",
        "flock", "floor", "flower", "fluid", "flush", "fly", "foam", "focus",
        "fog", "foil", "fold", "follow", "food", "foot", "force", "forest",
        "forget", "fork", "fortune", "forum", "forward", "fossil", "foster", "found",
        "fox", "fragile", "frame", "frequent", "friend", "fringe", "frog", "front",
        "frost", "frown", "frozen", "fruit", "fuel", "fun", "funny", "furnace",
        "fury", "future", "gadget", "gain", "galaxy", "gallery", "game", "gap",
        "garage", "garbage", "garden", "garlic", "garment", "gas", "gasp", "gate",
        "gather", "gauge", "gaze", "general", "genius", "genre", "gentle", "genuine",
        "gesture", "ghost", "giant", "gift", "giggle", "ginger", "giraffe", "girl",
        "give", "glad", "glance", "glare", "glass", "gleam", "globe", "gloom",
        "glory", "gloss", "glove", "glow", "glue", "goal", "goat", "goddess",
        "gold", "golf", "good", "goose", "gorgeous", "gown", "grab", "grace",
        "grade", "grain", "grandmother", "grant", "grape", "grasp", "grass", "grate",
        "grave", "gravy", "gray", "grease", "great", "green", "greet", "grief",
        "grill", "grin", "grind", "grip", "grit", "groan", "grocery", "groom",
        "gross", "group", "grove", "growl", "grown", "guard", "guess", "guest",
        "guide", "guilt", "guitar", "gun", "gust", "gutter", "guy", "habit",
        "hair", "half", "hall", "hammer", "hamster", "hand", "happy", "harbor",
        "hard", "harsh", "harvest", "hat", "hatch", "hate", "haul", "have",
        "hawk", "hazard", "head", "health", "heart", "heavy", "hedgehog", "height",
        "hello", "helmet", "help", "hen", "her", "herb", "here", "hero",
        "hesitate", "hide", "high", "hill", "hint", "hip", "hire", "historian",
        "hold", "hole", "holiday", "hollow", "home", "honey", "hood", "hope",
        "horizon", "horn", "horror", "horse", "hospital", "hotel", "hour", "hover",
        "hub", "huge", "human", "humble", "humor", "hundred", "hungry", "hunt",
        "hurdle", "hurry", "hurt", "husband", "hybrid", "ice", "icon", "idea",
        "ideal", "identity", "idle", "ignore", "ill", "illegal", "illness", "image",
        "imitate", "immense", "immune", "impact", "impose", "improve", "impulse", "inch",
        "include", "income", "increase", "index", "indicate", "indoor", "industry", "infant",
        "inflict", "inform", "inherit", "initial", "inject", "injury", "ink", "innate",
        "inner", "innocent", "input", "inquiry", "insect", "inside", "inspire", "install",
        "intact", "interest", "interior", "internal", "invade", "invest", "invite", "involve",
        "iron", "island", "isolate", "issue", "item", "ivory", "jacket", "jaguar",
        "jar", "jazz", "jealous", "jeans", "jelly", "jewel", "jigsaw", "job",
        "join", "joke", "jolly", "journey", "joy", "judge", "juice", "jump",
        "jungle", "junior", "junk", "just", "kangaroo", "keen", "keep", "ketchup",
        "key", "kick", "kid", "kidney", "king", "kiss", "kit", "kitchen",
        "kite", "kitty", "knee", "knife", "knight", "knit", "knob", "knock",
        "knot", "know", "knowledge", "known", "label", "labor", "lace", "ladder",
        "lady", "lake", "lamb", "lamp", "land", "landscape", "lane", "language",
        "laptop", "large", "laser", "last", "latch", "late", "later", "laugh",
        "laundry", "lava", "law", "lawn", "lawsuit", "layer", "lazy", "lead",
        "leader", "leaf", "leap", "learn", "lease", "least", "leather", "leave",
        "lecture", "left", "legal", "lemon", "lend", "length", "lens", "leopard",
        "lesson", "letter", "level", "lever", "liberty", "library", "license", "life",
        "lift", "light", "like", "limb", "limit", "linen", "liner", "linger",
        "lion", "list", "live", "liver", "living", "lizard", "load", "loan",
        "lobby", "local", "lock", "lodge", "loft", "logic", "lonely", "long",
        "look", "loose", "lorry", "lot", "loyal", "lucky", "luggage", "lumber",
        "lunar", "lunch", "lunge", "lyric", "machine", "magic", "magistrate", "magnet",
        "maid", "mail", "main", "maintain", "major", "make", "mammal", "manage",
        "mandate", "mango", "manner", "manual", "march", "margin", "marine", "market",
        "marriage", "mask", "mass", "mast", "master", "match", "mate", "material",
        "math", "matter", "mature", "mayor", "meadow", "mean", "measure", "meat",
        "mechanic", "medal", "media", "melon", "melt", "member", "memory", "mention",
        "mentor", "menu", "mercy", "merge", "merit", "merry", "mesh", "mess",
        "message", "metal", "meter", "method", "microphone", "middle", "midnight", "might",
        "mighty", "mild", "mile", "milk", "million", "mimic", "mind", "mine",
        "mineral", "minor", "mint", "minus", "minute", "miracle", "mirror", "misery",
        "miss", "mistake", "mix", "mixed", "mixture", "mobile", "model", "modify",
        "moist", "moment", "monarch", "money", "monkey", "monster", "month", "mood",
        "moon", "moral", "more", "morning", "mortal", "mortar", "mother", "motion",
        "motor", "motorcycle", "mount", "mouse", "mouth", "move", "movie", "much",
        "muck", "mural", "murder", "murky", "muscle", "museum", "mushroom", "music",
        "musical", "must", "mutual", "muzzle", "mystery", "myth", "nail", "naive",
        "name", "napkin", "narrow", "nasty", "nation", "native", "natural", "nature",
        "naval", "navigate", "near", "neck", "need", "negative", "neglect", "negotiate",
        "neighbor", "nerve", "nest", "network", "neutral", "news", "next", "nice",
        "night", "noble", "noise", "nominee", "none", "noon", "norm", "normal",
        "north", "nose", "notch", "notable", "note", "nothing", "notice", "notion",
        "novel", "nurse", "nylon", "oasis", "observe", "obtain", "occasion", "occupy",
        "ocean", "october", "odor", "offer", "office", "often", "olive", "olympic",
        "omit", "once", "one", "onion", "online", "only", "open", "opera",
        "opinion", "oppose", "option", "orange", "orbit", "orchard", "order", "organ",
        "origin", "orphan", "other", "outdoor", "outer", "output", "outrage", "outset",
        "oval", "oven", "over", "own", "owner", "oxide", "oxygen", "oyster",
        "ozone", "pacific", "package", "packet", "page", "paid", "pain", "paint",
        "pair", "palace", "palm", "panda", "panel", "panic", "paper", "parade",
        "parent", "park", "parrot", "party", "pass", "past", "paste", "patch",
        "patience", "patient", "patrol", "patron", "pattern", "pause", "pave", "payment",
        "peace", "peach", "peacock", "peak", "peanut", "pearl", "pedal", "penny",
        "people", "pepper", "percent", "perfect", "perch", "perform", "perhaps", "period",
        "permit", "person", "pest", "pet", "petal", "petty", "phase", "phone",
        "photo", "phrase", "physical", "piano", "pick", "picnic", "picture", "piece",
        "pigeon", "pillow", "pilot", "pin", "pink", "pioneer", "pipe", "pistol",
        "pitch", "pizza", "place", "plain", "plan", "plane", "planet", "plant",
        "plate", "platform", "play", "plaza", "plead", "please", "pledge", "pluck",
        "plumb", "plume", "plump", "plunge", "plural", "plus", "pocket", "poem",
        "poet", "point", "poise", "poison", "polar", "police", "pond", "pony",
        "pool", "popular", "portion", "position", "possess", "possible", "post", "pot",
        "potato", "potential", "poultry", "pound", "power", "practice", "praise", "predict",
        "prefer", "prefix", "pregnant", "premise", "premium", "prepare", "present", "preserve",
        "president", "press", "pressure", "prestige", "pretend", "pretty", "price", "pride",
        "priest", "primary", "prime", "print", "prior", "prison", "prize", "probe",
        "problem", "proceed", "process", "produce", "product", "profile", "program", "project",
        "promise", "promote", "prophet", "proposal", "prose", "protect", "protein", "protest",
        "proud", "prove", "provide", "province", "psychic", "public", "pudding", "pull",
        "pulse", "pumpkin", "punch", "pupil", "puppy", "purchase", "purse", "push",
        "puzzle", "python", "quality", "quantum", "quarter", "queen", "query", "quest",
        "quick", "quiet", "quilt", "quirk", "quota", "quote", "rabbit", "racial",
        "racism", "rack", "radar", "radio", "raft", "rail", "rain", "rainbow",
        "raise", "rally", "ranch", "random", "range", "rapid", "rare", "rather",
        "raven", "razor", "reach", "react", "read", "ready", "realm", "rebel",
        "recall", "receive", "recipe", "record", "recover", "reduce", "reflect", "reform",
        "refuse", "regard", "regime", "region", "regret", "regular", "reject", "relate",
        "relax", "release", "relief", "rely", "remain", "remark", "remedy", "remind",
        "remote", "remove", "render", "rent", "repair", "repeat", "replace", "report",
        "rescue", "resemble", "resist", "resort", "resource", "response", "rest", "restore",
        "result", "retail", "retain", "retire", "retreat", "return", "reveal", "review",
        "revolt", "reward", "rhythm", "rib", "ribbon", "rice", "rich", "ride",
        "ridge", "rifle", "right", "rigid", "rigor", "ring", "riot", "ripen",
        "rise", "risk", "ritual", "rival", "river", "roast", "robot", "robust",
        "rocket", "rock", "rocky", "romance", "roof", "room", "root", "rope",
        "rose", "rosy", "rotate", "rough", "round", "route", "rover", "royal",
        "rubber", "rugby", "ruler", "rumble", "rumor", "rural", "rust", "sack",
        "sacred", "saddle", "sadness", "safe", "sail", "saint", "salad", "salmon",
        "salon", "salary", "salute", "same", "sample", "sand", "satisfy", "satoshi",
        "sauce", "save", "say", "scale", "scam", "scan", "scare", "scene",
        "scent", "scheme", "scold", "scope", "score", "scorn", "scout", "scrap",
        "screen", "script", "scroll", "seal", "search", "season", "seat", "second",
        "secret", "section", "security", "seed", "seek", "seem", "segment", "seize",
        "select", "sell", "senate", "senior", "sense", "sentence", "series", "server",
        "settle", "setup", "seven", "sever", "shade", "shaft", "shake", "shall",
        "shame", "shape", "share", "shark", "sharp", "shave", "shed", "shell",
        "shelter", "shift", "shine", "ship", "shirt", "shock", "shoe", "shoot",
        "shop", "shore", "short", "shot", "should", "shoulder", "shout", "shove",
        "show", "shower", "shrimp", "shrine", "shrink", "shrug", "shut", "sibling",
        "sick", "side", "siege", "sight", "sign", "signal", "silence", "silent",
        "silk", "silly", "silver", "similar", "simple", "since", "singer", "single",
        "sink", "site", "siren", "sister", "situate", "six", "size", "skate",
        "skill", "skull", "slave", "sketch", "ski", "skirt", "slam", "slap",
        "slash", "slate", "slave", "sleep", "sleek", "slice", "slide", "slim",
        "slogan", "slope", "slot", "slow", "slush", "small", "smart", "smell",
        "smile", "smog", "smoke", "snake", "snap", "snare", "snarl", "sneak",
        "snow", "soak", "soap", "soar", "soccer", "social", "sock", "socket",
        "soda", "sofa", "soft", "software", "soil", "solar", "soldier", "solid",
        "solution", "solve", "some", "son", "song", "soon", "sophisticated", "sorry",
        "sort", "soul", "sound", "south", "space", "spare", "spark", "spatial",
        "speak", "spear", "special", "speed", "spell", "spend", "sphere", "spice",
        "spider", "spike", "spill", "spin", "spine", "spirit", "split", "spoil",
        "sponsor", "spoon", "sport", "spot", "spray", "spread", "spring", "spy",
        "squad", "square", "squeeze", "stadium", "staff", "stage", "stain", "stair",
        "stake", "stale", "stall", "stamp", "stand", "staple", "star", "start",
        "stash", "state", "station", "statue", "status", "steady", "steak", "steal",
        "steam", "steel", "steep", "steer", "stem", "step", "stew", "stick",
        "stiff", "still", "stock", "stomach", "stomp", "stone", "stool", "store",
        "storm", "story", "stout", "stove", "strap", "straw", "stray", "streak",
        "stream", "street", "strength", "stress", "stretch", "stride", "strike", "string",
        "strip", "stripe", "stroke", "strong", "struggle", "strut", "stub", "stuck",
        "student", "studio", "study", "stuff", "stumble", "stump", "stupid", "style",
        "subject", "submit", "subtle", "subway", "success", "such", "suck", "sudden",
        "suffer", "sugar", "suggest", "suit", "suite", "sulfur", "summer", "summit",
        "sun", "sunday", "sunlight", "sunny", "sunset", "super", "supply", "suppose",
        "supreme", "sure", "surface", "surge", "surprise", "surround", "survey", "suspect",
        "sustain", "swallow", "swamp", "swarm", "swear", "sweat", "sweep", "sweet",
        "swift", "swim", "swing", "switch", "sword", "syrup", "table", "tablet",
        "tackle", "tactic", "tail", "take", "tale", "talent", "talk", "tall",
        "tame", "tank", "tape", "target", "task", "taste", "tattoo", "taxi",
        "teach", "teacher", "team", "tear", "tease", "tech", "tempo", "ten",
        "tenant", "tender", "tennis", "tense", "tenth", "term", "terrace", "terrible",
        "test", "text", "than", "thank", "that", "theme", "then", "theory",
        "therapy", "there", "these", "thick", "thief", "thigh", "thing", "think",
        "third", "thirst", "thirteen", "thirty", "this", "thorn", "those", "three",
        "thrill", "thrive", "throat", "throne", "through", "throw", "thumb", "thunder",
        "thus", "ticket", "tiger", "tight", "timer", "tissue", "title", "toast",
        "tobacco", "today", "toddler", "toe", "together", "toilet", "token", "tomato",
        "tomorrow", "tone", "tongue", "tonight", "tool", "tooth", "topic", "torch",
        "tornado", "torpedo", "torrent", "total", "touch", "tough", "tour", "tourist",
        "toward", "tower", "town", "toxic", "toy", "trace", "track", "trade",
        "traffic", "tragic", "trail", "train", "trait", "trash", "travel", "tray",
        "treat", "treaty", "tree", "tremendous", "trend", "trial", "tribe", "tribute",
        "trick", "trigger", "trillion", "trim", "trip", "triumph", "troop", "tropical",
        "trouble", "trousers", "truck", "true", "truly", "trumpet", "trunk", "trust",
        "truth", "tulip", "tumor", "tuna", "tune", "tunnel", "turkey", "turn",
        "turtle", "twelve", "twenty", "twice", "twin", "twist", "two", "type",
        "typical", "ugly", "ultimate", "umbrella", "unable", "uncle", "under", "undergo",
        "unfair", "unfold", "unhappy", "uniform", "union", "unique", "unit", "unite",
        "universe", "unknown", "unless", "unlike", "unlock", "until", "unusual", "update",
        "upgrade", "uphold", "upon", "upper", "upset", "urban", "urge", "usage",
        "use", "used", "useful", "useless", "usual", "utility", "vacant", "vacuum",
        "vague", "valid", "valley", "valor", "value", "valve", "vampire", "van",
        "vanish", "vapor", "various", "vast", "vault", "veal", "vector", "velvet",
        "vendor", "venture", "venue", "verb", "verdict", "verify", "verse", "version",
        "versus", "very", "vessel", "veteran", "viable", "vibrant", "victim", "victory",
        "video", "view", "vigilant", "village", "vintage", "violin", "virtual", "virtue",
        "virus", "visa", "visible", "vision", "visit", "visual", "vital", "vivid",
        "vocal", "voice", "void", "volcano", "volume", "volunteer", "vote", "voyage",
        "wade", "wage", "wagon", "waist", "waste", "watch", "water", "wave",
        "weak", "wealth", "wealthy", "weapon", "wear", "weasel", "weather", "wedding",
        "weekend", "weigh", "welcome", "welfare", "west", "western", "whale", "what",
        "wheat", "wheel", "when", "where", "whether", "which", "while", "whisper",
        "whistle", "white", "whole", "whom", "whose", "wicked", "wide", "width",
        "wife", "wild", "will", "willing", "wind", "window", "wine", "wing",
        "wink", "winner", "winter", "wire", "wisdom", "wise", "wish", "witch",
        "withdraw", "within", "without", "witness", "wizard", "wobble", "woman", "wonder",
        "wood", "wool", "word", "work", "world", "worry", "worth", "worthwhile",
        "wrap", "wrath", "wreck", "wrestle", "wrist", "write", "wrong", "yacht",
        "year", "yellow", "yesterday", "yield", "young", "youth", "zebra", "zombie",
        "zone", "zoo", "zoom"
    };

    if (index >= 0 && index < BIP39_WORDLIST_SIZE) {
        return BIP39_WORDLIST[index];
    }
    return "";
}

static int GetBIP39Index(const std::string& word) {
    for (int i = 0; i < BIP39_WORDLIST_SIZE; ++i) {
        if (std::string(GetBIP39Word(i)) == word) {
            return i;
        }
    }
    return -1;
}

CWallet::CWallet()
    : m_locked(true)
    , m_encrypted(false)
    , m_dirty(false)
    , m_use_mldsa(true)
    , m_min_fee(DEFAULT_TX_FEE)
    , m_default_sigtype(SignatureType::Schnorr)
{
}

CWallet::~CWallet() {
}

std::unique_ptr<CWallet> CWallet::Create(const std::string& wallet_path) {
    auto wallet = std::make_unique<CWallet>();
    wallet->m_wallet_path = wallet_path;
    return wallet;
}

std::unique_ptr<CWallet> CWallet::Load(const std::string& wallet_path, const std::string& passphrase) {
    auto wallet = std::make_unique<CWallet>();
    wallet->m_wallet_path = wallet_path;
    if (!wallet->LoadWallet(passphrase)) {
        return nullptr;
    }
    return wallet;
}

bool CWallet::CreateWallet(const std::string& passphrase) {
    try {
        std::vector<std::string> mnemonic_words;
        if (!ComputeMnemonic(mnemonic_words)) {
            return false;
        }

        if (!ComputeMasterKey(mnemonic_words)) {
            return false;
        }

        if (!passphrase.empty()) {
            if (!m_master_key.GetKey().empty()) {
                EncryptWallet(passphrase);
            }
        }

        if (!SetupHDChain()) {
            return false;
        }

        m_default_pubkey = m_master_key.GetPubKey();
        m_dirty = true;
        m_locked = false;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating wallet: " << e.what() << std::endl;
        return false;
    }
}

bool CWallet::LoadWallet(const std::string& passphrase) {
    try {
        CWalletDB db(m_wallet_path);
        std::vector<uint8_t> crypted_key, salt;
        unsigned int rounds = 0;
        if (!db.ReadMasterKey(crypted_key, salt, rounds)) {
            return false;
        }
        m_master_key.m_crypted_key = crypted_key;

        std::vector<uint8_t> master_key;
        if (!m_master_key.Decrypt(passphrase, master_key)) {
            return false;
        }

        m_master_key.SetPrivKey(master_key);
        m_default_pubkey = m_master_key.GetPubKey();

        m_encrypted = true;
        m_locked = false;
        m_dirty = false;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading wallet: " << e.what() << std::endl;
        return false;
    }
}

bool CWallet::EncryptWallet(const std::string& passphrase) {
    if (m_encrypted) {
        return false;
    }

    if (m_master_key.GetKey().empty()) {
        return false;
    }

    CCrypter crypter;
    std::vector<uint8_t> crypted_key;
    if (!crypter.Encrypt(m_master_key.GetKey(), passphrase, crypted_key)) {
        return false;
    }

    m_encrypted = true;
    return true;
}

bool CWallet::Unlock(const std::string& passphrase) {
    if (!m_encrypted) {
        m_locked = false;
        return true;
    }

    std::vector<uint8_t> master_key;
    if (!m_master_key.Decrypt(passphrase, master_key)) {
        return false;
    }

    if (!m_master_key.SetPrivKey(master_key)) {
        return false;
    }

    m_locked = false;
    return true;
}

void CWallet::Lock() {
    if (m_encrypted) {
        m_locked = true;
    }
}

CPubKey CWallet::GenerateNewKey(OutputType type, bool internal) {
    CKey key;
    key.MakeNewKey(true);

    CPubKey pubkey = key.GetPubKey();

    if (!AddKey(key)) {
        return CPubKey();
    }

    CKeyPool keypool;
    keypool.vchPubKey = pubkey;
    keypool.output_type = type;
    keypool.nCreationTime = time(nullptr);

    m_key_pools.push_back(keypool);

    return pubkey;
}

CPubKey CWallet::GetPubKey(const CKeyID& address) const {
    auto it = m_pubkeys.find(address);
    if (it != m_pubkeys.end()) {
        return it->second;
    }
    return CPubKey();
}

bool CWallet::GetKey(const CKeyID& address, CKey& key) const {
    auto it = m_keys.find(address);
    if (it != m_keys.end()) {
        key = it->second;
        return true;
    }
    return false;
}

std::string CWallet::GetDefaultAddress() const {
    if (m_default_pubkey.IsValid()) {
        CKeyID keyid = m_default_pubkey.GetID();
        return keyid.ToString();
    }
    return "";
}

bool CWallet::AddKey(const CKey& key) {
    if (key.GetPubKey().IsValid()) {
        CKeyID keyid = key.GetPubKey().GetID();
        m_keys[keyid] = key;
        m_pubkeys[keyid] = key.GetPubKey();
        m_pubkey_to_keyid[key.GetPubKey()] = keyid;
        m_dirty = true;
        return true;
    }
    return false;
}

bool CWallet::AddKeyPubKey(const CKey& key, const CPubKey& pubkey) {
    if (pubkey.IsValid() && pubkey.GetID() == key.GetPubKey().GetID()) {
        return AddKey(key);
    }
    return false;
}

bool CWallet::CreateBip84Path(std::vector<uint32_t>& path, CoinType coin, bool internal) {
    path.clear();
    path.push_back(0x8000002C);
    path.push_back(0x80000000 | static_cast<uint32_t>(coin));
    path.push_back(0x80000000 | 0);
    path.push_back(internal ? 1 : 0);
    path.push_back(0);
    path.push_back(0);
    return true;
}

bool CWallet::DeriveKeyPath(CKey& key, const std::vector<uint32_t>& path) {
    if (m_master_key.GetKey().empty()) {
        return false;
    }

    CKey derived_key = m_master_key.GetCKey();

    for (size_t i = 0; i < path.size(); ++i) {
        uint32_t child_index = path[i];
        bool hardened = (child_index & 0x80000000) != 0;
        child_index &= 0x7FFFFFFF;

        CKey child_key;

        if (!derived_key.Derive(child_key, child_index, hardened)) {
            return false;
        }

        derived_key = child_key;
    }

    key = derived_key;
    return true;
}

std::string CWallet::CreateTransaction(
    const std::vector<CRecipient>& recipients,
    CTransactionRef& tx,
    uint256& wtxid,
    int64_t& fee,
    std::string& fail_reason,
    bool sign
) {
    try {
        auto newTx = std::make_shared<CTransaction>();
        newTx->version = 2;
        newTx->locktime = 0;

        CAmount total_amount = 0;

        for (const auto& recipient : recipients) {
            TxOut out;
            out.nValue = recipient.nAmount;
            out.scriptPubKey = recipient.scriptPubKey;
            newTx->vout.push_back(out);
            total_amount += recipient.nAmount;
        }

        if (sign) {
            std::map<COutPoint, Coin> empty_coins;
            if (!SignTransaction(newTx, empty_coins)) {
                fail_reason = "Signing failed";
                return "";
            }
        }

        tx = newTx;
        wtxid = newTx->GetHash();

        return wtxid.ToString();
    } catch (const std::exception& e) {
        fail_reason = e.what();
        return "";
    }
}

bool CWallet::SignTransaction(std::shared_ptr<CTransaction> tx, std::map<COutPoint, Coin>& coins) {
    for (size_t i = 0; i < tx->vin.size(); ++i) {
        const auto& txin = tx->vin[i];

        auto coin_it = coins.find(txin.prevout);
        if (coin_it == coins.end()) {
            continue;
        }

        const Coin& coin = coin_it->second;

        CKey key;
        CKeyID keyID(coin.out.scriptPubKey.GetID());
        if (!GetKey(keyID, key)) {
            continue;
        }

        uint256 hash = tx->GetHash();

        std::vector<uint8_t> signature;
        std::vector<uint8_t> pubkey = key.GetPubKey().m_key;

        unsigned char sig[72];
        unsigned int siglen = 72;

        EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
        if (!ec_key) {
            continue;
        }

        BIGNUM* bn = BN_new();
        BN_bin2bn(key.GetKey().data(), 32, bn);

        EC_KEY_set_private_key(ec_key, bn);

        unsigned char hash32[32];
        memcpy(hash32, hash.m_data, 32);

        if (ECDSA_sign(0, hash32, 32, sig, &siglen, ec_key)) {
            signature.assign(sig, sig + siglen);
            signature.push_back(0x01);
            signature.insert(signature.end(), pubkey.begin(), pubkey.end());
            tx->vin[i].scriptSig << signature;
        }

        BN_free(bn);
        EC_KEY_free(ec_key);
    }

    return true;
}

bool CWallet::CommitTransaction(CTransactionRef tx, mapValue_t mapValue) {
    uint256 txid = tx->GetHash();

    CWalletTx wtx;
    wtx.m_tx_hash = txid;
    wtx.m_time = time(nullptr);
    wtx.m_status = "sending";
    wtx.m_order_pos = -1;

    m_wallet_txs[txid] = wtx;

    return true;
}

CTransactionRef CWallet::GetTransaction(const uint256& txid) const {
    auto it = m_wallet_txs.find(txid);
    if (it != m_wallet_txs.end()) {
        return it->second.m_tx;
    }
    return nullptr;
}

CAmount CWallet::GetBalance() const {
    CAmount total = 0;
    for (const auto& tx : m_wallet_txs) {
        total += tx.second.m_credit - tx.second.m_debit;
    }
    return total;
}

CAmount CWallet::GetSpendableBalance() const {
    return GetBalance();
}

CAmount CWallet::GetUnconfirmedBalance() const {
    CAmount total = 0;
    for (const auto& tx : m_wallet_txs) {
        if (tx.second.m_confirmations < 6) {
            total += tx.second.m_credit;
        }
    }
    return total;
}

std::vector<COutput> CWallet::AvailableCoins(bool fOnlySafe) {
    std::vector<COutput> coins;
    return coins;
}

bool CWallet::SelectCoins(
    const CAmount& target,
    CAmount& value_ret,
    std::vector<COutput>& setCoinsRet,
    std::set<CInputCoin>& setCoinsSelectedRet
) {
    std::vector<COutput> coins = AvailableCoins(true);

    CAmount value = 0;
    setCoinsRet.clear();
    setCoinsSelectedRet.clear();

    for (const auto& coin : coins) {
        setCoinsRet.push_back(coin);
        setCoinsSelectedRet.insert(CInputCoin(coin.tx, coin.i));
        value += coin.value;

        if (value >= target) {
            value_ret = value;
            return true;
        }
    }

    value_ret = value;
    return value >= target;
}

std::vector<std::string> CWallet::GetAddressesByAccount(const std::string& account) {
    std::vector<std::string> addresses;
    for (const auto& entry : m_pubkeys) {
        addresses.push_back(entry.second.ToString());
    }
    return addresses;
}

bool CWallet::ComputeMnemonic(std::vector<std::string>& words) {
    words.resize(12);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, BIP39_WORDLIST_SIZE - 1);

    for (int i = 0; i < 12; ++i) {
        int index = dis(gen);
        words[i] = GetBIP39Word(index);
    }

    return true;
}

bool CWallet::ComputeMasterKey(const std::vector<std::string>& words) {
    std::string entropy;
    for (const auto& word : words) {
        entropy += word;
        entropy += " ";
    }

    std::vector<uint8_t> seed(64);
    HMAC(EVP_sha512(), "BHTWallet", 9,
         reinterpret_cast<const uint8_t*>(entropy.data()), entropy.size(),
         seed.data(), nullptr);

    std::vector<uint8_t> master_key(seed.begin(), seed.begin() + 32);

    m_master_seed = seed;
    m_master_key.SetPrivKey(master_key);

    return true;
}

void CWallet::DeriveChildKey(
    const CKey& parent_key,
    const CPubKey& parent_pubkey,
    uint32_t child_index,
    CKey& child_key,
    CPubKey& child_pubkey
) {
    std::vector<uint8_t> data;

    bool hardened = (child_index & 0x80000000) != 0;
    child_index &= 0x7FFFFFFF;

    if (hardened) {
        data.push_back(0x00);
        data.insert(data.end(), parent_key.GetKey().begin(), parent_key.GetKey().end());
    } else {
        data.insert(data.end(), parent_pubkey.m_key.begin(), parent_pubkey.m_key.end());
    }

    uint8_t index_bytes[4];
    index_bytes[0] = (child_index >> 24) & 0xFF;
    index_bytes[1] = (child_index >> 16) & 0xFF;
    index_bytes[2] = (child_index >> 8) & 0xFF;
    index_bytes[3] = child_index & 0xFF;
    data.insert(data.end(), index_bytes, index_bytes + 4);

    std::vector<uint8_t> hash(64);
    HMAC(EVP_sha512(), "BHTWallet", 9,
         data.data(), data.size(), hash.data(), nullptr);

    std::vector<uint8_t> child_key_data(hash.begin(), hash.begin() + 32);
    for (size_t i = 0; i < 32; ++i) {
        child_key_data[i] ^= parent_key.GetKey()[i];
    }

    child_key.SetPrivKey(child_key_data);
    child_pubkey = child_key.GetPubKey();
}

bool CWallet::SetupHDChain(bool internal) {
    std::vector<uint32_t> path;
    if (!CreateBip84Path(path, CoinType::BHC, internal)) {
        return false;
    }

    CKey key;
    return DeriveKeyPath(key, path);
}

bool CWallet::ComputeBip32Path(
    const std::string& purpose,
    CoinType coin,
    bool internal,
    std::vector<uint32_t>& path
) {
    path.clear();

    if (purpose == "84") {
        path.push_back(0x80000054);
    } else if (purpose == "49") {
        path.push_back(0x80000031);
    } else {
        path.push_back(0x8000002C);
    }

    path.push_back(0x80000000 | static_cast<uint32_t>(coin));
    path.push_back(0x80000000 | 0);
    path.push_back(internal ? 0x80000001 : 0x80000000);
    path.push_back(0);
    path.push_back(0);

    return true;
}

bool CWallet::BackupWallet(const std::string& filename) {
    try {
        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "Failed to open backup file: " << filename << std::endl;
            return false;
        }

        file << "BHTWALLETBACKUP_V1\n";

        uint32_t version = 1;
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));

        uint64_t timestamp = static_cast<uint64_t>(time(nullptr));
        file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));

        uint32_t keyCount = static_cast<uint32_t>(m_keys.size());
        file.write(reinterpret_cast<const char*>(&keyCount), sizeof(keyCount));

        for (const auto& keyPair : m_keys) {
            CKeyID keyID = keyPair.first;
            const CKey& key = keyPair.second;

            file.write(reinterpret_cast<const char*>(&keyID), sizeof(keyID));

            std::vector<uint8_t> keyData = key.GetKey();
            uint32_t keySize = static_cast<uint32_t>(keyData.size());
            file.write(reinterpret_cast<const char*>(&keySize), sizeof(keySize));
            file.write(reinterpret_cast<const char*>(keyData.data()), keySize);
        }

        uint64_t txCount = static_cast<uint64_t>(m_wallet_txs.size());
        file.write(reinterpret_cast<const char*>(&txCount), sizeof(txCount));

        for (const auto& txPair : m_wallet_txs) {
            uint256 txHash = txPair.first;
            file.write(reinterpret_cast<const char*>(&txHash), sizeof(txHash));

            const CWalletTx& wtx = txPair.second;
            uint64_t wtxTime = wtx.m_time;
            file.write(reinterpret_cast<const char*>(&wtxTime), sizeof(wtxTime));
        }

        uint8_t encrypted = m_encrypted ? 1 : 0;
        file.write(reinterpret_cast<const char*>(&encrypted), sizeof(encrypted));

        file.close();

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error backing up wallet: " << e.what() << std::endl;
        return false;
    }
}

bool CWallet::ExportWallet(const std::string& filename) {
    return BackupWallet(filename);
}

bool CWallet::ImportWallet(const std::string& filename) {
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open wallet file: " << filename << std::endl;
            return false;
        }

        std::string header;
        std::getline(file, header);
        if (header != "BHTWALLETBACKUP_V1") {
            std::cerr << "Invalid wallet backup format" << std::endl;
            file.close();
            return false;
        }

        uint32_t version;
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != 1) {
            std::cerr << "Unsupported wallet backup version: " << version << std::endl;
            file.close();
            return false;
        }

        uint64_t timestamp;
        file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));

        uint32_t keyCount;
        file.read(reinterpret_cast<char*>(&keyCount), sizeof(keyCount));

        for (uint32_t i = 0; i < keyCount; ++i) {
            CKeyID keyID;
            file.read(reinterpret_cast<char*>(&keyID), sizeof(keyID));

            uint32_t keySize;
            file.read(reinterpret_cast<char*>(&keySize), sizeof(keySize));

            std::vector<uint8_t> keyData(keySize);
            file.read(reinterpret_cast<char*>(keyData.data()), keySize);

            CKey key;
            key.SetPrivKey(keyData);
            AddKey(key);
        }

        uint64_t txCount;
        file.read(reinterpret_cast<char*>(&txCount), sizeof(txCount));

        for (uint64_t i = 0; i < txCount; ++i) {
            uint256 txHash;
            file.read(reinterpret_cast<char*>(&txHash), sizeof(txHash));

            uint64_t wtxTime;
            file.read(reinterpret_cast<char*>(&wtxTime), sizeof(wtxTime));
        }

        uint8_t encrypted;
        file.read(reinterpret_cast<char*>(&encrypted), sizeof(encrypted));
        m_encrypted = (encrypted == 1);

        file.close();
        m_dirty = true;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error importing wallet: " << e.what() << std::endl;
        return false;
    }
}

std::string CWallet::GetMnemonic() {
    if (m_master_seed.empty()) {
        std::vector<std::string> words;
        if (!ComputeMnemonic(words)) {
            return "";
        }

        std::string mnemonic;
        for (size_t i = 0; i < words.size(); ++i) {
            if (i > 0) mnemonic += " ";
            mnemonic += words[i];
        }
        return mnemonic;
    }

    return "";
}

bool CWallet::RestoreFromMnemonic(const std::string& mnemonic) {
    try {
        std::istringstream iss(mnemonic);
        std::vector<std::string> words;
        std::string word;

        while (iss >> word) {
            words.push_back(word);
        }

        if (words.size() != 12 && words.size() != 24) {
            std::cerr << "Invalid mnemonic length: " << words.size() << std::endl;
            return false;
        }

        for (const auto& w : words) {
            if (GetBIP39Index(w) < 0) {
                std::cerr << "Invalid word in mnemonic: " << w << std::endl;
                return false;
            }
        }

        if (!ComputeMasterKey(words)) {
            return false;
        }

        if (!SetupHDChain()) {
            return false;
        }

        m_default_pubkey = m_master_key.GetPubKey();
        m_dirty = true;
        m_locked = false;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error restoring wallet from mnemonic: " << e.what() << std::endl;
        return false;
    }
}

}
}
