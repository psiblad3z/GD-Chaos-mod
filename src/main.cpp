#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <deque>
#include <unordered_map>

using namespace geode::prelude;

struct ClickPacket {
    float timer;
    bool down;
    int button;
    bool isP1;
};

static bool g_lagBypass = false;
static bool g_lagActive = false;
static PlayLayer* g_lagOwner = nullptr;
static bool g_doubleNext = false;
static bool g_silenceActive = false;
static bool g_noclipActive = false;
static bool g_curseNext = false;
static float g_savedMusicVol = -1.0f;
static float g_savedSfxVol = -1.0f;

static std::string eventToSetting(const std::string& eventName) {
    static const std::unordered_map<std::string, std::string> map = {
        {"MOON (LOW GRAV)", "enable-moon"},
        {"HEAVY (HIGH GRAV)", "enable-heavy"},
        {"MINI", "enable-mini"},
        {"BIG", "enable-big"},
        {"SHIP/WAVE FLIP", "enable-shipflip"},
        {"GEOGD JUMPSCARE", "enable-jumpscare"},
        {"SLIDE / 30 FPS", "enable-30fps"},
        {"HOLY INPUT LAG", "enable-inputlag"},
        {"EARTHQUAKE", "enable-earthquake"},
        {"DISCO PARTY", "enable-disco"},
        {"WHERE AM I?", "enable-whereami"},
        {"TURN THE LIGHT ON!", "enable-dark"},
        {"FAKE DEATH", "enable-fakedeath"},
        {"TEXT SPAM", "enable-textspam"},
        {"RANDOM JUMP", "enable-randomjump"},
        {"DOUBLE EVENT", "enable-doubleevent"},
        {"FAKE AD", "enable-fakead"},
        {"SILENCE", "enable-silence"},
        {"FAKE CRASH", "enable-fakecrash"},
        {"FLASHBANG", "enable-flashbang"},
        {"NOTHING", "enable-nothing"},
        {"FORCED NOCLIP", "enable-noclip"},
        {"SUICIDE", "enable-suicide"},
        {"CURSE", "enable-curse"},
        {"EARRAPE", "enable-earrape"},
        {"CREDITS", "enable-credits"}
    };
    auto it = map.find(eventName);
    return it != map.end() ? it->second : "";
}

class InputLagNode : public CCNode {
public:
    PlayLayer* m_targetLayer = nullptr;
    std::deque<ClickPacket> m_packets;
    float m_duration = 10.0f;

    static InputLagNode* create(PlayLayer* pl, float duration) {
        auto node = new InputLagNode();
        if (node && node->init()) {
            node->m_targetLayer = pl;
            node->m_duration = duration;
            node->autorelease();
            node->scheduleUpdate();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    void pushClick(bool down, int button, bool isP1) {
        m_packets.push_back(ClickPacket{0.3f, down, button, isP1});
    }

    void update(float dt) override {
        if (!m_targetLayer || PlayLayer::get() != m_targetLayer) {
            removeFromParentAndCleanup(true);
            return;
        }

        m_duration -= dt;
        for (auto& p : m_packets) p.timer -= dt;

        while (!m_packets.empty() && m_packets.front().timer <= 0.f) {
            auto p = m_packets.front();
            m_packets.pop_front();
            g_lagBypass = true;
            m_targetLayer->handleButton(p.down, p.button, p.isP1);
            g_lagBypass = false;
        }

        if (m_duration <= 0.f) {
            while (!m_packets.empty()) {
                auto p = m_packets.front();
                m_packets.pop_front();
                g_lagBypass = true;
                m_targetLayer->handleButton(p.down, p.button, p.isP1);
                g_lagBypass = false;
            }
            removeFromParentAndCleanup(true);
        }
    }

    void onExit() override {
        g_lagActive = false;
        g_lagOwner = nullptr;
        CCNode::onExit();
    }
};

class $modify(LagHook, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (g_lagBypass) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }
        auto pl = typeinfo_cast<PlayLayer*>(this);
        if (!pl || !g_lagActive || g_lagOwner != pl || button != static_cast<int>(PlayerButton::Jump)) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        auto scene = CCDirector::sharedDirector()->getRunningScene();
        auto node = scene ? scene->getChildByTag(8899) : nullptr;
        if (auto lagNode = typeinfo_cast<InputLagNode*>(node)) {
            lagNode->pushClick(down, button, isPlayer1);
            return;
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

class FpsChaosNode : public CCNode {
public:
    float m_timeLeft = 10.0f;
    double m_oldInterval = 0.0;

    static FpsChaosNode* create(float duration) {
        auto node = new FpsChaosNode();
        if (node && node->init()) {
            node->m_timeLeft = duration;
            node->autorelease();
            node->scheduleUpdate();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    void onEnter() override {
        CCNode::onEnter();
        if (auto dir = CCDirector::sharedDirector()) {
            m_oldInterval = dir->getAnimationInterval();
            dir->setAnimationInterval(1.0 / 20.0);
        }
    }

    void update(float dt) override {
        m_timeLeft -= dt;
        if (auto dir = CCDirector::sharedDirector()) {
            if (dir->getAnimationInterval() != 1.0 / 20.0) {
                dir->setAnimationInterval(1.0 / 20.0);
            }
        }
        if (m_timeLeft <= 0.f) removeFromParentAndCleanup(true);
    }

    void onExit() override {
        if (auto dir = CCDirector::sharedDirector()) {
            dir->setAnimationInterval(m_oldInterval);
        }
        CCNode::onExit();
    }
};

class TextSpamNode : public CCNode {
public:
    float m_timeLeft = 12.0f;
    float m_spawnTimer = 0.0f;

    static TextSpamNode* create(float duration) {
        auto node = new TextSpamNode();
        if (node && node->init()) {
            node->m_timeLeft = duration;
            node->autorelease();
            node->scheduleUpdate();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    void update(float dt) override {
        m_timeLeft -= dt;
        m_spawnTimer -= dt;

        if (m_spawnTimer <= 0.f) {
            m_spawnTimer = 0.04f + (rand() % 40) / 200.f;
            spawnLabel();
        }

        if (m_timeLeft <= 0.f) removeFromParentAndCleanup(true);
    }

    void spawnLabel() {
        static const std::vector<std::string> texts = {
            "do u see me?", "heeey", "GO!", "meow", "psiblade was here",
            "dont die ok?", "im here", "xdd", "skill issue",
            "hardest - stereo madness", "u're him.", "u got this",
            "wtf how", "u need help?"
        };

        auto& txt = texts[rand() % texts.size()];
        auto label = CCLabelBMFont::create(txt.c_str(), "bigFont.fnt");
        if (!label) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        float x = 30.f + rand() % (int)(winSize.width - 60);
        float y = 30.f + rand() % (int)(winSize.height - 60);
        label->setPosition({x, y});
        label->setOpacity(51);
        label->setScale(0.35f + (rand() % 100) / 250.f);
        label->setRotation((rand() % 60) - 30);

        this->addChild(label);

        label->runAction(CCSequence::create(
            CCDelayTime::create(2.5f),
            CCFadeTo::create(0.8f, 0),
            CCRemoveSelf::create(),
            nullptr
        ));
    }
};

class RandomJumpNode : public CCNode {
public:
    PlayLayer* m_targetLayer = nullptr;

    static RandomJumpNode* create(PlayLayer* pl) {
        auto node = new RandomJumpNode();
        if (node && node->init()) {
            node->m_targetLayer = pl;
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    void onEnter() override {
        CCNode::onEnter();
        this->runAction(CCSequence::create(
            CCDelayTime::create(0.3f),
            CCCallFunc::create(this, callfunc_selector(RandomJumpNode::doJump)),
            nullptr
        ));
    }

    void doJump() {
        if (!m_targetLayer || PlayLayer::get() != m_targetLayer) {
            removeFromParentAndCleanup(true);
            return;
        }

        int btn = static_cast<int>(PlayerButton::Jump);
        g_lagBypass = true;
        m_targetLayer->handleButton(true, btn, true);
        m_targetLayer->handleButton(false, btn, true);
        g_lagBypass = false;

        showLabel();

        this->runAction(CCSequence::create(
            CCDelayTime::create(2.5f),
            CCCallFunc::create(this, callfunc_selector(RandomJumpNode::selfRemove)),
            nullptr
        ));
    }

    void showLabel() {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (!scene) return;

        if (auto old = scene->getChildByTag(9966)) old->removeFromParent();

        auto label = CCLabelBMFont::create("sorry missclick 8)", "bigFont.fnt");
        if (!label) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        label->setPosition({winSize.width / 2, winSize.height * 0.65f});
        label->setScale(0.6f);
        label->setOpacity(220);
        label->setTag(9966);
        scene->addChild(label);

        label->runAction(CCSequence::create(
            CCDelayTime::create(2.0f),
            CCFadeTo::create(0.3f, 0),
            CCRemoveSelf::create(),
            nullptr
        ));
    }

    void selfRemove() {
        removeFromParentAndCleanup(true);
    }
};

class FakeAdNode : public CCNode {
public:
    static FakeAdNode* create(float x, float y, float delay) {
        auto node = new FakeAdNode();
        if (node && node->init()) {
            node->autorelease();
            node->setPosition({x, y});
            node->m_delay = delay;
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    void onEnter() override {
        CCNode::onEnter();
        this->setVisible(false);
        this->runAction(CCSequence::create(
            CCDelayTime::create(m_delay),
            CCCallFunc::create(this, callfunc_selector(FakeAdNode::show)),
            nullptr
        ));
    }

    void show() {
        this->setVisible(true);

        if (auto eng = FMODAudioEngine::sharedEngine()) {
            eng->playEffect("achievement_01.ogg");
        }

        float w = 170.f;
        float h = 120.f;

        auto border = CCLayerColor::create({0, 0, 0, 255}, w + 4, h + 4);
        border->setPosition({-w / 2 - 2, -h / 2 - 2});
        border->setAnchorPoint({0, 0});
        this->addChild(border, 0);

        auto bg = CCLayerColor::create({240, 240, 240, 255}, w, h);
        bg->setPosition({-w / 2, -h / 2});
        bg->setAnchorPoint({0, 0});
        this->addChild(bg, 1);

        auto titleBar = CCLayerColor::create({70, 130, 220, 255}, w, 18);
        titleBar->setPosition({-w / 2, h / 2 - 18});
        titleBar->setAnchorPoint({0, 0});
        this->addChild(titleBar, 2);

        auto title = CCLabelBMFont::create("ADVERTISEMENT", "bigFont.fnt");
        title->setScale(0.26f);
        title->setPosition({0, h / 2 - 9});
        this->addChild(title, 3);

        auto line1 = CCLabelBMFont::create("SUPER DLC FOR", "bigFont.fnt");
        line1->setScale(0.28f);
        line1->setPosition({0, 25});
        line1->setColor({0, 0, 0});
        this->addChild(line1, 3);

        auto line2 = CCLabelBMFont::create("\"GD CHAOS MOD\"", "bigFont.fnt");
        line2->setScale(0.3f);
        line2->setPosition({0, 6});
        line2->setColor({180, 0, 0});
        this->addChild(line2, 3);

        auto line3 = CCLabelBMFont::create("ONLY 17.99$!!!", "bigFont.fnt");
        line3->setScale(0.28f);
        line3->setPosition({0, -13});
        line3->setColor({0, 0, 0});
        this->addChild(line3, 3);

        auto btnBg = CCLayerColor::create({40, 180, 60, 255}, 110, 24);
        btnBg->setPosition({-55, -h / 2 + 8});
        btnBg->setAnchorPoint({0, 0});
        this->addChild(btnBg, 2);

        auto btnText = CCLabelBMFont::create("BUY NOW!!!", "bigFont.fnt");
        btnText->setScale(0.3f);
        btnText->setPosition({0, -h / 2 + 20});
        btnText->setColor({255, 255, 255});
        this->addChild(btnText, 3);

        this->runAction(CCSequence::create(
            CCDelayTime::create(2.5f),
            CCFadeOut::create(0.4f),
            CCRemoveSelf::create(),
            nullptr
        ));
    }

    float m_delay = 0.0f;
};

class FakeCrashNode : public CCNode {
public:
    static FakeCrashNode* create() {
        auto node = new FakeCrashNode();
        if (node && node->init()) {
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    void onEnter() override {
        CCNode::onEnter();

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto black = CCLayerColor::create({15, 15, 15, 255}, winSize.width * 2, winSize.height * 2);
        black->setPosition({-winSize.width / 2, -winSize.height / 2});
        this->addChild(black, 0);

        auto title = CCLabelBMFont::create("Geometry Dash has stopped working", "bigFont.fnt");
        title->setScale(0.55f);
        title->setPosition({winSize.width / 2, winSize.height / 2 + 40});
        title->setColor({255, 255, 255});
        this->addChild(title, 1);

        auto sub = CCLabelBMFont::create("A problem caused the program to stop working correctly.", "chatFont.fnt");
        sub->setScale(0.7f);
        sub->setPosition({winSize.width / 2, winSize.height / 2});
        sub->setColor({210, 210, 210});
        this->addChild(sub, 1);

        auto sub2 = CCLabelBMFont::create("Windows will close the program and notify you", "chatFont.fnt");
        sub2->setScale(0.7f);
        sub2->setPosition({winSize.width / 2, winSize.height / 2 - 22});
        sub2->setColor({210, 210, 210});
        this->addChild(sub2, 1);

        auto sub3 = CCLabelBMFont::create("if a solution is available.", "chatFont.fnt");
        sub3->setScale(0.7f);
        sub3->setPosition({winSize.width / 2, winSize.height / 2 - 44});
        sub3->setColor({210, 210, 210});
        this->addChild(sub3, 1);

        this->runAction(CCSequence::create(
            CCDelayTime::create(1.8f),
            CCFadeOut::create(0.4f),
            CCRemoveSelf::create(),
            nullptr
        ));
    }
};

class CreditsNode : public CCNode {
public:
    static CreditsNode* create() {
        auto node = new CreditsNode();
        if (node && node->init()) {
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    void onEnter() override {
        CCNode::onEnter();

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto bg = CCLayerColor::create({0, 0, 0, 110}, winSize.width * 2, winSize.height * 2);
        bg->setPosition({-winSize.width / 2, -winSize.height / 2});
        this->addChild(bg, 0);

        auto container = CCNode::create();
        container->setPosition({winSize.width / 2, -60.f});
        this->addChild(container, 1);

        auto l1 = CCLabelBMFont::create("mod by psiblade", "bigFont.fnt");
        l1->setScale(0.8f);
        l1->setPosition({0, 0});
        container->addChild(l1);

        auto l2 = CCLabelBMFont::create("thanks for playing", "bigFont.fnt");
        l2->setScale(0.7f);
        l2->setPosition({0, -50});
        container->addChild(l2);

        auto l3 = CCLabelBMFont::create("the trial period is over", "bigFont.fnt");
        l3->setScale(0.6f);
        l3->setColor({255, 60, 60});
        l3->setPosition({0, -110});
        container->addChild(l3);

        auto l4a = CCLabelBMFont::create("u can buy another 10 min", "bigFont.fnt");
        l4a->setScale(0.5f);
        l4a->setPosition({0, -170});
        container->addChild(l4a);

        auto l4b = CCLabelBMFont::create("for only 49,99$", "bigFont.fnt");
        l4b->setScale(0.55f);
        l4b->setColor({255, 220, 0});
        l4b->setPosition({0, -210});
        container->addChild(l4b);

        auto l4c = CCLabelBMFont::create("..............just kidding lol", "bigFont.fnt");
        l4c->setScale(0.4f);
        l4c->setColor({180, 180, 180});
        l4c->setPosition({0, -255});
        container->addChild(l4c);

        container->runAction(CCMoveTo::create(18.0f, {winSize.width / 2, winSize.height + 500.f}));

        this->runAction(CCSequence::create(
            CCDelayTime::create(15.0f),
            CCFadeOut::create(0.8f),
            CCRemoveSelf::create(),
            nullptr
        ));
    }
};

struct EventDef {
    std::string name;
    float duration;
    std::function<void(PlayLayer*)> onStart;
    std::function<void(PlayLayer*)> onEnd;
};

struct ActiveEvent {
    int defIndex;
    CCLabelBMFont* label;
    CCLayerColor* border;
    CCLayerColor* barBg;
    CCLayerColor* barFg;
};

class $modify(MyPlayLayer, PlayLayer) {

    struct Fields {
        CCLayerColor* topBarFg = nullptr;
        CCLabelBMFont* countdownLabel = nullptr;
        int countdownValue = 5;
        std::vector<ActiveEvent> activeEvents;
    };

    std::vector<EventDef> getEventList() {
        return {
            {
                "MOON (LOW GRAV)", 15.0f,
                [](PlayLayer* pl) {
                    if (pl->m_player1) pl->m_player1->m_gravity = pl->m_player1->m_gravity * 0.7f;
                    if (pl->m_player2) pl->m_player2->m_gravity = pl->m_player2->m_gravity * 0.7f;
                },
                [](PlayLayer* pl) {
                    if (pl->m_player1) pl->m_player1->m_gravity = pl->m_player1->m_gravity / 0.7f;
                    if (pl->m_player2) pl->m_player2->m_gravity = pl->m_player2->m_gravity / 0.7f;
                }
            },
            {
                "HEAVY (HIGH GRAV)", 15.0f,
                [](PlayLayer* pl) {
                    if (pl->m_player1) pl->m_player1->m_gravity = pl->m_player1->m_gravity * 1.4f;
                    if (pl->m_player2) pl->m_player2->m_gravity = pl->m_player2->m_gravity * 1.4f;
                },
                [](PlayLayer* pl) {
                    if (pl->m_player1) pl->m_player1->m_gravity = pl->m_player1->m_gravity / 1.4f;
                    if (pl->m_player2) pl->m_player2->m_gravity = pl->m_player2->m_gravity / 1.4f;
                }
            },
            {
                "MINI", 15.0f,
                [](PlayLayer* pl) {
                    if (pl->m_player1) pl->m_player1->togglePlayerScale(true, false);
                    if (pl->m_player2) pl->m_player2->togglePlayerScale(true, false);
                },
                [](PlayLayer* pl) {
                    if (pl->m_player1) pl->m_player1->togglePlayerScale(false, false);
                    if (pl->m_player2) pl->m_player2->togglePlayerScale(false, false);
                }
            },
            {
                "BIG", 15.0f,
                [](PlayLayer* pl) {
                    if (pl->m_player1) pl->m_player1->togglePlayerScale(false, false);
                    if (pl->m_player2) pl->m_player2->togglePlayerScale(false, false);
                },
                [](PlayLayer* pl) {}
            },
            {
                "SHIP/WAVE FLIP", 10.0f,
                [](PlayLayer* pl) {
                    if (pl->m_player1 && (pl->m_player1->m_isShip || pl->m_player1->m_isDart))
                        pl->m_player1->m_isUpsideDown = !pl->m_player1->m_isUpsideDown;
                    if (pl->m_player2 && (pl->m_player2->m_isShip || pl->m_player2->m_isDart))
                        pl->m_player2->m_isUpsideDown = !pl->m_player2->m_isUpsideDown;
                },
                [](PlayLayer* pl) {
                    if (pl->m_player1 && (pl->m_player1->m_isShip || pl->m_player1->m_isDart))
                        pl->m_player1->m_isUpsideDown = !pl->m_player1->m_isUpsideDown;
                    if (pl->m_player2 && (pl->m_player2->m_isShip || pl->m_player2->m_isDart))
                        pl->m_player2->m_isUpsideDown = !pl->m_player2->m_isUpsideDown;
                }
            },
            {
                "GEOGD JUMPSCARE", 10.0f,
                [](PlayLayer* pl) {
                    FMODAudioEngine::sharedEngine()->playEffect("is-it-possible-with-accurate-hitboxes.mp3"_spr);
                    if (pl->m_player1) {
                        auto faceSprite = CCSprite::create("GEOGD.png"_spr);
                        if (faceSprite) {
                            faceSprite->setTag(7711);
                            faceSprite->setScale(0.75f);
                            pl->m_player1->addChild(faceSprite, 100);
                            auto size = pl->m_player1->getContentSize();
                            faceSprite->setPosition({size.width / 2, size.height / 2});
                        }
                    }
                },
                [](PlayLayer* pl) {
                    if (pl->m_player1) {
                        auto faceSprite = pl->m_player1->getChildByTag(7711);
                        if (faceSprite) faceSprite->removeFromParent();
                    }
                }
            },
            {
                "SLIDE / 30 FPS", 10.0f,
                [](PlayLayer* pl) {
                    auto scene = CCDirector::sharedDirector()->getRunningScene();
                    if (scene && !scene->getChildByTag(7766)) {
                        auto node = FpsChaosNode::create(10.0f);
                        node->setTag(7766);
                        scene->addChild(node);
                    }
                },
                [](PlayLayer* pl) {}
            },
            {
                "HOLY INPUT LAG", 10.0f,
                [](PlayLayer* pl) {
                    auto scene = CCDirector::sharedDirector()->getRunningScene();
                    if (!scene) return;

                    g_lagActive = true;
                    g_lagOwner = pl;

                    if (!scene->getChildByTag(8899)) {
                        auto lagNode = InputLagNode::create(pl, 10.0f);
                        lagNode->setTag(8899);
                        scene->addChild(lagNode);
                    }

                    auto winSize = CCDirector::sharedDirector()->getWinSize();
                    auto lagBorder = CCLayerColor::create({255, 200, 0, 50}, winSize.width * 3, winSize.height * 3);
                    lagBorder->setTag(9988);
                    pl->addChild(lagBorder, 895);
                },
                [](PlayLayer* pl) {
                    g_lagActive = false;
                    g_lagOwner = nullptr;
                    auto lagBorder = pl->getChildByTag(9988);
                    if (lagBorder) lagBorder->removeFromParent();
                }
            },
            {
                "EARTHQUAKE", 12.0f,
                [](PlayLayer* pl) {
                    pl->shakeCamera(4.0f, 3.0f, 0.05f);
                },
                [](PlayLayer* pl) {}
            },
            {
                "DISCO PARTY", 10.0f,
                [](PlayLayer* pl) {
                    auto winSize = CCDirector::sharedDirector()->getWinSize();
                    auto disco = CCLayerColor::create({255, 0, 0, 150}, winSize.width * 3, winSize.height * 3);
                    disco->setTag(8888);
                    pl->addChild(disco, 899);

                    disco->runAction(CCRepeatForever::create(CCSequence::create(
                        CCTintTo::create(0.1f, 255, 0, 255),
                        CCFadeTo::create(0.1f, 50),
                        CCTintTo::create(0.1f, 0, 255, 255),
                        CCFadeTo::create(0.1f, 200),
                        CCTintTo::create(0.1f, 255, 255, 0),
                        CCFadeTo::create(0.1f, 100),
                        CCTintTo::create(0.1f, 0, 255, 0),
                        CCFadeTo::create(0.1f, 250),
                        nullptr
                    )));
                },
                [](PlayLayer* pl) {
                    auto disco = pl->getChildByTag(8888);
                    if (disco) disco->removeFromParent();
                }
            },
            {
                "WHERE AM I?", 15.0f,
                [](PlayLayer* pl) {
                    if (pl->m_player1) {
                        pl->m_player1->stopAllActions();
                        pl->m_player1->setVisible(true);
                        pl->m_player1->runAction(CCBlink::create(15.0f, 30));
                    }
                },
                [](PlayLayer* pl) {
                    if (pl->m_player1) {
                        pl->m_player1->stopAllActions();
                        pl->m_player1->setVisible(true);
                    }
                }
            },
            {
                "TURN THE LIGHT ON!", 10.0f,
                [](PlayLayer* pl) {
                    auto winSize = CCDirector::sharedDirector()->getWinSize();
                    auto dark = CCLayerColor::create({0, 0, 0, 0}, winSize.width * 3, winSize.height * 3);
                    dark->setTag(7777);
                    pl->addChild(dark, 900);
                    dark->runAction(CCFadeTo::create(0.3f, 235));
                },
                [](PlayLayer* pl) {
                    auto dark = pl->getChildByTag(7777);
                    if (dark) {
                        dark->runAction(CCSequence::create(
                            CCFadeTo::create(0.3f, 0),
                            CCCallFuncN::create(pl, callfuncN_selector(PlayLayer::removeChild)),
                            nullptr
                        ));
                    }
                }
            },
            {
                "FAKE DEATH", 5.0f,
                [](PlayLayer* pl) {
                    if (!pl || !pl->m_player1) return;

                    if (auto eng = FMODAudioEngine::sharedEngine()) {
                        eng->playEffect("explode_11.ogg");
                    }

                    auto winSize = CCDirector::sharedDirector()->getWinSize();
                    auto flash = CCLayerColor::create({255, 50, 50, 180}, winSize.width * 2, winSize.height * 2);
                    flash->setTag(9911);
                    pl->addChild(flash, 900);
                    flash->runAction(CCSequence::create(
                        CCFadeTo::create(0.3f, 0),
                        CCCallFuncN::create(pl, callfuncN_selector(MyPlayLayer::removeNode)),
                        nullptr
                    ));

                    pl->shakeCamera(10.0f, 0.3f, 0.05f);

                    pl->m_player1->setVisible(false);
                    if (pl->m_player2) pl->m_player2->setVisible(false);

                    pl->runAction(CCSequence::create(
                        CCDelayTime::create(0.4f),
                        CCCallFunc::create(pl, callfunc_selector(MyPlayLayer::fakeReappear)),
                        nullptr
                    ));
                },
                [](PlayLayer* pl) {}
            },
            {
                "TEXT SPAM", 12.0f,
                [](PlayLayer* pl) {
                    auto scene = CCDirector::sharedDirector()->getRunningScene();
                    if (scene && !scene->getChildByTag(9900)) {
                        auto node = TextSpamNode::create(12.0f);
                        node->setTag(9900);
                        scene->addChild(node);
                    }
                },
                [](PlayLayer* pl) {}
            },
            {
                "RANDOM JUMP", 5.0f,
                [](PlayLayer* pl) {
                    auto scene = CCDirector::sharedDirector()->getRunningScene();
                    if (scene && !scene->getChildByTag(9901)) {
                        auto node = RandomJumpNode::create(pl);
                        node->setTag(9901);
                        scene->addChild(node);
                    }
                },
                [](PlayLayer* pl) {}
            },
            {
                "DOUBLE EVENT", 5.0f,
                [](PlayLayer* pl) {
                    g_doubleNext = true;
                },
                [](PlayLayer* pl) {
                    g_doubleNext = false;
                }
            },
            {
                "FAKE AD", 6.0f,
                [](PlayLayer* pl) {
                    auto scene = CCDirector::sharedDirector()->getRunningScene();
                    if (!scene) return;

                    auto winSize = CCDirector::sharedDirector()->getWinSize();

                    float positions[3][2] = {
                        { winSize.width * 0.15f, winSize.height * 0.75f },
                        { winSize.width * 0.80f, winSize.height * 0.55f },
                        { winSize.width * 0.35f, winSize.height * 0.30f }
                    };

                    for (int i = 0; i < 3; i++) {
                        auto node = FakeAdNode::create(positions[i][0], positions[i][1], i * 0.4f);
                        if (!node) continue;
                        node->setTag(9902 + i);
                        scene->addChild(node, 9999);
                    }
                },
                [](PlayLayer* pl) {}
            },
            {
                "SILENCE", 5.0f,
                [](PlayLayer* pl) {
                    if (auto eng = FMODAudioEngine::sharedEngine()) {
                        eng->pauseAllEffects();
                        eng->pauseMusic(0);
                        g_silenceActive = true;
                    }
                },
                [](PlayLayer* pl) {
                    if (auto eng = FMODAudioEngine::sharedEngine()) {
                        eng->resumeAllEffects();
                        eng->resumeMusic(0);
                        g_silenceActive = false;
                    }
                }
            },
            {
                "FAKE CRASH", 3.5f,
                [](PlayLayer* pl) {
                    if (auto eng = FMODAudioEngine::sharedEngine()) {
                        eng->pauseAllEffects();
                        eng->pauseMusic(0);
                        g_silenceActive = true;
                    }

                    auto scene = CCDirector::sharedDirector()->getRunningScene();
                    if (scene && !scene->getChildByTag(9905)) {
                        auto node = FakeCrashNode::create();
                        node->setTag(9905);
                        scene->addChild(node, 9999);
                    }
                },
                [](PlayLayer* pl) {
                    if (auto eng = FMODAudioEngine::sharedEngine()) {
                        eng->resumeAllEffects();
                        eng->resumeMusic(0);
                        g_silenceActive = false;
                    }
                }
            },
            {
                "FLASHBANG", 3.0f,
                [](PlayLayer* pl) {
                    if (auto eng = FMODAudioEngine::sharedEngine()) {
                        eng->playEffect("flashbang_explode1.mp3"_spr);
                    }

                    auto winSize = CCDirector::sharedDirector()->getWinSize();
                    auto white = CCLayerColor::create({255, 255, 255, 255}, winSize.width * 2, winSize.height * 2);
                    white->setTag(9920);
                    pl->addChild(white, 99999);
                    white->runAction(CCSequence::create(
                        CCDelayTime::create(0.6f),
                        CCFadeTo::create(1.8f, 0),
                        CCRemoveSelf::create(),
                        nullptr
                    ));
                },
                [](PlayLayer* pl) {}
            },
            {
                "NOTHING", 5.0f,
                [](PlayLayer* pl) {
                    auto winSize = CCDirector::sharedDirector()->getWinSize();
                    auto label = CCLabelBMFont::create("NOTHING", "bigFont.fnt");
                    label->setScale(0.8f);
                    label->setPosition({winSize.width / 2, winSize.height / 2});
                    label->setOpacity(180);
                    label->setTag(9960);
                    pl->addChild(label, 9999);

                    label->runAction(CCSequence::create(
                        CCDelayTime::create(2.5f),
                        CCFadeTo::create(0.5f, 0),
                        CCRemoveSelf::create(),
                        nullptr
                    ));
                },
                [](PlayLayer* pl) {}
            },
            {
                "FORCED NOCLIP", 5.0f,
                [](PlayLayer* pl) {
                    g_noclipActive = true;
                },
                [](PlayLayer* pl) {
                    g_noclipActive = false;
                }
            },
            {
                "SUICIDE", 3.0f,
                [](PlayLayer* pl) {
                    if (!pl || !pl->m_player1) return;

                    auto winSize = CCDirector::sharedDirector()->getWinSize();
                    auto label = CCLabelBMFont::create("im tired of jumping..", "bigFont.fnt");
                    label->setScale(0.7f);
                    label->setPosition({winSize.width / 2, winSize.height / 2});
                    label->setTag(9977);
                    pl->addChild(label, 9999);

                    pl->runAction(CCSequence::create(
                        CCDelayTime::create(1.2f),
                        CCCallFunc::create(pl, callfunc_selector(MyPlayLayer::doSuicide)),
                        nullptr
                    ));
                },
                [](PlayLayer* pl) {}
            },
            {
                "CURSE", 3.0f,
                [](PlayLayer* pl) {
                    g_curseNext = true;

                    auto winSize = CCDirector::sharedDirector()->getWinSize();
                    auto red = CCLayerColor::create({150, 0, 0, 60}, winSize.width * 2, winSize.height * 2);
                    red->setTag(9930);
                    pl->addChild(red, 998);
                    red->runAction(CCSequence::create(
                        CCDelayTime::create(1.5f),
                        CCFadeTo::create(0.5f, 0),
                        CCRemoveSelf::create(),
                        nullptr
                    ));

                    auto label = CCLabelBMFont::create("CURSED", "bigFont.fnt");
                    label->setScale(0.9f);
                    label->setPosition({winSize.width / 2, winSize.height * 0.8f});
                    label->setColor({255, 50, 50});
                    label->setTag(9931);
                    pl->addChild(label, 9999);
                    label->runAction(CCSequence::create(
                        CCDelayTime::create(1.5f),
                        CCFadeTo::create(0.5f, 0),
                        CCRemoveSelf::create(),
                        nullptr
                    ));
                },
                [](PlayLayer* pl) {}
            },
            {
                "EARRAPE", 5.0f,
                [](PlayLayer* pl) {
                    if (auto eng = FMODAudioEngine::sharedEngine()) {
                        g_savedMusicVol = eng->getBackgroundMusicVolume();
                        g_savedSfxVol = eng->getEffectsVolume();
                        eng->setBackgroundMusicVolume(1.0f);
                        eng->setEffectsVolume(1.0f);
                    }

                    auto winSize = CCDirector::sharedDirector()->getWinSize();
                    auto label = CCLabelBMFont::create("LOUDER!!!", "bigFont.fnt");
                    label->setScale(1.1f);
                    label->setPosition({winSize.width / 2, winSize.height * 0.75f});
                    label->setColor({255, 60, 60});
                    label->setTag(9936);
                    pl->addChild(label, 9999);
                    label->runAction(CCSequence::create(
                        CCDelayTime::create(1.2f),
                        CCFadeTo::create(0.5f, 0),
                        CCRemoveSelf::create(),
                        nullptr
                    ));
                },
                [](PlayLayer* pl) {
                    if (auto eng = FMODAudioEngine::sharedEngine()) {
                        if (g_savedMusicVol >= 0.f) eng->setBackgroundMusicVolume(g_savedMusicVol);
                        if (g_savedSfxVol >= 0.f) eng->setEffectsVolume(g_savedSfxVol);
                        g_savedMusicVol = -1.0f;
                        g_savedSfxVol = -1.0f;
                    }
                }
            },
            {
                "CREDITS", 7.0f,
                [](PlayLayer* pl) {
                    auto scene = CCDirector::sharedDirector()->getRunningScene();
                    if (scene && !scene->getChildByTag(9945)) {
                        auto node = CreditsNode::create();
                        node->setTag(9945);
                        scene->addChild(node, 9999);
                    }
                },
                [](PlayLayer* pl) {}
            }
        };
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (g_noclipActive) return;
        PlayLayer::destroyPlayer(player, object);
    }

    void doSuicide() {
        if (!this || !m_player1) return;
        bool saved = g_noclipActive;
        g_noclipActive = false;
        this->destroyPlayer(m_player1, nullptr);
        g_noclipActive = saved;
    }

    void removeNode(CCNode* node) {
        if (node) node->removeFromParent();
    }

    void fakeReappear() {
        if (m_player1) m_player1->setVisible(true);
        if (m_player2) m_player2->setVisible(true);
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        float barWidth = 200.0f;
        float barHeight = 8.0f;
        float xPos = winSize.width / 2 - barWidth / 2;
        float yPos = winSize.height - 25;

        auto border = CCLayerColor::create({0, 0, 0, 255}, barWidth + 3, barHeight + 3);
        border->setPosition({xPos - 1.5f, yPos - 1.5f});
        border->setAnchorPoint({0, 0});
        this->addChild(border, 999);

        auto bg = CCLayerColor::create({30, 30, 30, 255}, barWidth, barHeight);
        bg->setPosition({xPos, yPos});
        bg->setAnchorPoint({0, 0});
        this->addChild(bg, 1000);

        m_fields->topBarFg = CCLayerColor::create({255, 220, 0, 255}, barWidth, barHeight);
        m_fields->topBarFg->setPosition({xPos, yPos});
        m_fields->topBarFg->setAnchorPoint({0, 0});
        this->addChild(m_fields->topBarFg, 1001);

        m_fields->countdownLabel = CCLabelBMFont::create("5", "bigFont.fnt");
        m_fields->countdownLabel->setScale(0.35f);
        m_fields->countdownLabel->setPosition({winSize.width / 2, yPos - 12});
        this->addChild(m_fields->countdownLabel, 1000);

        this->runAction(CCSequence::create(
            CCDelayTime::create(0.6f),
            CCCallFunc::create(this, callfunc_selector(MyPlayLayer::startMainTimer)),
            nullptr
        ));
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        if (!this) return;

        g_lagActive = false;
        g_lagOwner = nullptr;
        g_doubleNext = false;
        g_noclipActive = false;
        g_curseNext = false;

        if (g_silenceActive) {
            if (auto eng = FMODAudioEngine::sharedEngine()) {
                eng->resumeAllEffects();
                eng->resumeMusic(0);
            }
            g_silenceActive = false;
        }

        if (g_savedMusicVol >= 0.f || g_savedSfxVol >= 0.f) {
            if (auto eng = FMODAudioEngine::sharedEngine()) {
                if (g_savedMusicVol >= 0.f) eng->setBackgroundMusicVolume(g_savedMusicVol);
                if (g_savedSfxVol >= 0.f) eng->setEffectsVolume(g_savedSfxVol);
            }
            g_savedMusicVol = -1.0f;
            g_savedSfxVol = -1.0f;
        }

        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (scene) {
            if (auto n = scene->getChildByTag(9900)) n->removeFromParent();
            if (auto n = scene->getChildByTag(9901)) n->removeFromParent();
            if (auto n = scene->getChildByTag(7766)) n->removeFromParent();
            if (auto n = scene->getChildByTag(8899)) n->removeFromParent();
            if (auto n = scene->getChildByTag(9966)) n->removeFromParent();
            if (auto n = scene->getChildByTag(9902)) n->removeFromParent();
            if (auto n = scene->getChildByTag(9903)) n->removeFromParent();
            if (auto n = scene->getChildByTag(9904)) n->removeFromParent();
            if (auto n = scene->getChildByTag(9905)) n->removeFromParent();
            if (auto n = scene->getChildByTag(9945)) n->removeFromParent();
        }

        if (auto n = this->getChildByTag(7777)) n->removeFromParent();
        if (auto n = this->getChildByTag(9911)) n->removeFromParent();
        if (auto n = this->getChildByTag(9920)) n->removeFromParent();
        if (auto n = this->getChildByTag(9960)) n->removeFromParent();
        if (auto n = this->getChildByTag(9977)) n->removeFromParent();
        if (auto n = this->getChildByTag(9988)) n->removeFromParent();
        if (auto n = this->getChildByTag(9930)) n->removeFromParent();
        if (auto n = this->getChildByTag(9931)) n->removeFromParent();
        if (auto n = this->getChildByTag(9936)) n->removeFromParent();

        auto events = getEventList();
        for (auto& ev : m_fields->activeEvents) {
            events[ev.defIndex].onEnd(this);
            if (ev.label) ev.label->removeFromParent();
            if (ev.border) ev.border->removeFromParent();
            if (ev.barBg) ev.barBg->removeFromParent();
            if (ev.barFg) ev.barFg->removeFromParent();
        }
        m_fields->activeEvents.clear();

        this->unschedule(schedule_selector(MyPlayLayer::tickCountdown));

        if (m_fields->topBarFg) {
            m_fields->topBarFg->stopAllActions();
        }

        startMainTimer();
    }

    void startMainTimer() {
        if (!this || !m_fields->topBarFg) return;
        m_fields->topBarFg->setScaleX(1.0f);
        m_fields->topBarFg->runAction(CCSequence::create(
            CCScaleTo::create(5.0f, 0.0f, 1.0f),
            CCCallFunc::create(this, callfunc_selector(MyPlayLayer::onMainTimerDone)),
            nullptr
        ));

        this->unschedule(schedule_selector(MyPlayLayer::tickCountdown));
        m_fields->countdownValue = 5;
        m_fields->countdownLabel->setString("5");
        this->schedule(schedule_selector(MyPlayLayer::tickCountdown), 1.0f);
    }

    void tickCountdown(float dt) {
        if (!this || !m_fields->countdownLabel) return;
        m_fields->countdownValue--;
        if (m_fields->countdownValue <= 0) {
            this->unschedule(schedule_selector(MyPlayLayer::tickCountdown));
            return;
        }
        m_fields->countdownLabel->setString(std::to_string(m_fields->countdownValue).c_str());
    }

    void onMainTimerDone() {
        if (!this) return;
        startRandomEvent();
        startMainTimer();
    }

    void startRandomEvent() {
        if (!this) return;

        bool doDouble = g_doubleNext;
        g_doubleNext = false;

        spawnOneEvent(false);
        if (doDouble) spawnOneEvent(true);
    }

    void spawnOneEvent(bool excludeDouble) {
        auto events = getEventList();
        auto mod = Mod::get();

        std::vector<int> enabled;
        for (size_t i = 0; i < events.size(); i++) {
            std::string sid = eventToSetting(events[i].name);
            if (sid.empty()) {
                enabled.push_back((int)i);
                continue;
            }
            if (mod->getSettingValue<bool>(sid)) {
                enabled.push_back((int)i);
            }
        }

        if (enabled.empty()) return;

        bool hasMiniOrBig = false;
        bool hasGravityEvent = false;

        for (const auto& ev : m_fields->activeEvents) {
            if (ev.defIndex == 2 || ev.defIndex == 3) hasMiniOrBig = true;
            if (ev.defIndex == 0 || ev.defIndex == 1) hasGravityEvent = true;
        }

        int doubleIdx = -1;
        int suicideIdx = -1;
        for (size_t i = 0; i < events.size(); i++) {
            if (events[i].name == "DOUBLE EVENT") doubleIdx = (int)i;
            if (events[i].name == "SUICIDE") suicideIdx = (int)i;
        }

        int index = 0;
        int attempts = 0;
        do {
            index = enabled[rand() % enabled.size()];
            attempts++;
            if (hasMiniOrBig && (index == 2 || index == 3)) continue;
            if (hasGravityEvent && (index == 0 || index == 1)) continue;
            if (excludeDouble && index == doubleIdx) continue;
            if (index == suicideIdx && (rand() % 100) < 82) continue;
            break;
        } while (attempts < 30);

        auto& def = events[index];
        std::string nameCopy = def.name;
        float durCopy = def.duration;

        if (g_curseNext) {
            durCopy *= 2.0f;
            g_curseNext = false;
        }

        def.onStart(this);
        addActiveEvent(index, nameCopy, durCopy);
    }

    void layoutEvents() {
        if (!this) return;
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        float xPos = winSize.width - 140;

        for (size_t i = 0; i < m_fields->activeEvents.size(); i++) {
            float yPos = winSize.height - 90 - (i * 40);
            auto& ev = m_fields->activeEvents[i];
            if (ev.label) ev.label->setPosition({xPos, yPos + 14});
            if (ev.border) ev.border->setPosition({xPos - 1, yPos - 1});
            if (ev.barBg) ev.barBg->setPosition({xPos, yPos});
            if (ev.barFg) ev.barFg->setPosition({xPos, yPos});
        }
    }

    void addActiveEvent(int defIndex, std::string name, float duration) {
        if (!this) return;

        ActiveEvent ev;
        ev.defIndex = defIndex;

        ev.label = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
        if (!ev.label) return;
        ev.label->setScale(0.4f);
        ev.label->setAnchorPoint({0, 0});
        this->addChild(ev.label, 1000);

        ev.border = CCLayerColor::create({120, 120, 120, 255}, 122, 10);
        if (!ev.border) return;
        ev.border->setAnchorPoint({0, 0});
        this->addChild(ev.border, 999);

        ev.barBg = CCLayerColor::create({30, 30, 30, 200}, 120, 8);
        if (!ev.barBg) return;
        ev.barBg->setAnchorPoint({0, 0});
        this->addChild(ev.barBg, 1000);

        auto barFg = CCLayerColor::create({200, 200, 200, 255}, 120, 8);
        if (!barFg) return;
        barFg->setAnchorPoint({0, 0});
        this->addChild(barFg, 1001);
        ev.barFg = barFg;

        m_fields->activeEvents.push_back(ev);
        layoutEvents();

        ev.barFg->runAction(CCSequence::create(
            CCScaleTo::create(duration, 0.0f, 1.0f),
            CCCallFuncN::create(this, callfuncN_selector(MyPlayLayer::onEventDone)),
            nullptr
        ));
    }

    void onEventDone(CCNode* sender) {
        if (!this) return;
        auto events = getEventList();
        for (size_t i = 0; i < m_fields->activeEvents.size(); i++) {
            if (m_fields->activeEvents[i].barFg == sender) {
                auto& ev = m_fields->activeEvents[i];
                events[ev.defIndex].onEnd(this);
                if (ev.label) ev.label->removeFromParent();
                if (ev.border) ev.border->removeFromParent();
                if (ev.barBg) ev.barBg->removeFromParent();
                if (ev.barFg) ev.barFg->removeFromParent();
                m_fields->activeEvents.erase(m_fields->activeEvents.begin() + i);
                break;
            }
        }
        layoutEvents();
    }
};
