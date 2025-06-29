/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <RotarySliderWithLabels.h>
#include <Utilities.h>

static juce::String getDSPOptionName(Project13AudioProcessor::DSP_Option option)
{
    switch (option)
    {
        case Project13AudioProcessor::DSP_Option::Phase:
            return "PHASE";
        case Project13AudioProcessor::DSP_Option::Chorus:
            return "CHORUS";
        case Project13AudioProcessor::DSP_Option::Overdrive:
            return "OVERDRIVE";
        case Project13AudioProcessor::DSP_Option::LadderFilter:
            return "LADDERFILTER";
        case Project13AudioProcessor::DSP_Option::GeneralFilter:
            return "GEN FILTER";
        case Project13AudioProcessor::DSP_Option::END_OF_LIST:
            jassertfalse;
    }
    
    return "NO SELECTION";
}

static Project13AudioProcessor::DSP_Option getDSPOptionFromName( juce::String name )
{
    if( name == "PHASE" )
        return Project13AudioProcessor::DSP_Option::Phase;
    if( name == "CHORUS" )
        return Project13AudioProcessor::DSP_Option::Chorus;
    if( name == "OVERDRIVE" )
        return Project13AudioProcessor::DSP_Option::Overdrive;
    if( name == "LADDERFILTER" )
        return Project13AudioProcessor::DSP_Option::LadderFilter;
    if( name == "GEN FILTER" )
        return Project13AudioProcessor::DSP_Option::GeneralFilter;
    
    return Project13AudioProcessor::DSP_Option::END_OF_LIST;
}
//==============================================================================
HorizontalConstrainer::HorizontalConstrainer(std::function<juce::Rectangle<int>()> confinerBoundsGetter,
                                             std::function<juce::Rectangle<int>()> confineeBoundsGetter)
                                                 :
boundsToConfineToGetter(std::move(confinerBoundsGetter)),
boundsOfConfineeGetter(std::move(confineeBoundsGetter))

{
    
}
    
void HorizontalConstrainer::checkBounds (juce::Rectangle<int>& bounds,
                                         const juce::Rectangle<int>& previousBounds,
                                         const juce::Rectangle<int>& limits,
                                         bool isStretchingTop,
                                         bool isStretchingLeft,
                                         bool isStretchingBottom,
                                         bool isStretchingRight)
{
    /*
     'bounds' is the bounding box that we are TRYING to set componentToConfine to.
     we only want to support horizontal dragging within the TabButtonBar.
     
     so, retain the existing Y position given to the TabBarButton by the TabbedButtonBar when the button was created.
     */
    
    bounds.setY( previousBounds.getY() );
    
    /*
     the X position needs to be limited to the left and right side of the owning TabbedButtonBar however, to prevent the right
     side of the TabBarButton from being dragged outside the bounds to the TabbedButtonBar, we must subtract the width of this button
     from the right side of the TabbedButtonBar
     
     in order for this to work, we need to know the bounds of both the TabbedButtonBar and the TabBarButton.
     hence, loose coupling using lambda getter functions via the constructor parameters.
     Loose coupling is preferred vs tight coupling.
     */
    
    
    if( boundsToConfineToGetter != nullptr && boundsOfConfineeGetter != nullptr )
    {
        auto boundsToConfineTo = boundsToConfineToGetter();
        auto boundsOfConfinee = boundsOfConfineeGetter();
        
        bounds.setX( juce::jlimit(boundsToConfineTo.getX(), boundsToConfineTo.getRight() - boundsOfConfinee.getWidth(),
                                  bounds.getX()));
    }
    else
    {
        bounds.setX(juce::jlimit(limits.getX(),
                                limits.getY(),
                                bounds.getX()));
    }
}

//==============================================================================
ExtendedTabBarButton::ExtendedTabBarButton(const juce::String& name,
                                           juce::TabbedButtonBar& owner,
                                           Project13AudioProcessor::DSP_Option dspOption) :
juce::TabBarButton(name, owner),
option(dspOption)
{
    constrainer = std::make_unique<HorizontalConstrainer>([&owner](){ return owner.getLocalBounds(); },
                                                          [this](){ return getBounds(); });
    constrainer->setMinimumOnscreenAmounts(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
}

int ExtendedTabBarButton::getBestTabLength(int depth)
{
    auto bestWidth = getLookAndFeel().getTabButtonBestWidth(*this, depth);
    
    auto& bar = getTabbedButtonBar();
    /*
     we want the tabs to occupy the entire TabBar width.
     so, after computing the best width for the button and depth,
     we choose whichever value is bigger, the bestWidth, or an equal division of the bar's width based on
     the number of tabs in the bar.
     */
    return juce::jmax(bestWidth,
                      bar.getWidth() / bar.getNumTabs());
}

void ExtendedTabBarButton::mouseDown (const juce::MouseEvent& e)
{
    toFront(true);
    dragger.startDraggingComponent (this, e);
    juce::TabBarButton::mouseDown(e);
}
//==============================================================================
void ExtendedTabBarButton::mouseDrag (const juce::MouseEvent& e)
{
    dragger.dragComponent (this, e, constrainer.get());
}
//==============================================================================
ExtendedTabbedButtonBar::ExtendedTabbedButtonBar() :
    juce::TabbedButtonBar(juce::TabbedButtonBar::Orientation::TabsAtTop)
{
    auto img = juce::Image(juce::Image::PixelFormat::SingleChannel, 1, 1, true);
    auto gfx = juce::Graphics(img);
    gfx.fillAll(juce::Colours::transparentBlack);
    
    dragImage = juce::ScaledImage(img, 1.0);
}

bool ExtendedTabbedButtonBar::isInterestedInDragSource (const SourceDetails& dragSourceDetails)
{
    if( dynamic_cast<ExtendedTabBarButton*>( dragSourceDetails.sourceComponent.get() ) )
        return true;
    
    return false;
}

void ExtendedTabbedButtonBar::itemDragEnter(const SourceDetails &dragSourceDetails)
{
    DBG("ExtendedTabbedButtonBar::itemDragEnter");
    juce::DragAndDropTarget::itemDragEnter(dragSourceDetails);
}

juce::Array<juce::TabBarButton*> ExtendedTabbedButtonBar::getTabs()
{
    auto numTabs = getNumTabs();
    auto tabs = juce::Array<juce::TabBarButton*>();
    tabs.resize(numTabs);
    for( int i = 0; i < numTabs; ++i )
    {
        tabs.getReference(i) = getTabButton(i);
    }
    return tabs;
}

int ExtendedTabbedButtonBar::findDraggedItemIndex(const SourceDetails &dragSourceDetails)

{
    if( auto tabButtonBeingDragged = dynamic_cast<ExtendedTabBarButton*>(
        dragSourceDetails.sourceComponent.get() ) )
    {
        auto tabs = getTabs();
        
        auto idx = tabs.indexOf(tabButtonBeingDragged);
        return idx;
    }
    
    return -1;
}

juce::TabBarButton* ExtendedTabbedButtonBar::findDraggedItem(const SourceDetails &dragSourceDetails)
{
    return getTabButton( findDraggedItemIndex(dragSourceDetails) );
}

void ExtendedTabbedButtonBar::itemDragMove(const SourceDetails &dragSourceDetails)
{
    if( auto tabButtonBeingDragged = dynamic_cast<ExtendedTabBarButton*>(
        dragSourceDetails.sourceComponent.get() ) )
    {
        auto idx = findDraggedItemIndex(dragSourceDetails);
        if( idx == -1)
        {
            DBG( "failed to find tab being dragged in list of tabs");
            jassertfalse;
            return;
        }
        
        // find the tab that tabButtonBeingDragged is colliding with.
        // it might be on the right
        // it might be on the left
        // if it's on the right,
        // if tabButtonBeingDragged's x is > nextTab.getX() + nextTab.getWidth() * 0.5, swap their position
        
        auto previousTabIndex = idx - 1;
        auto nextTabIndex = idx + 1;
        auto previousTab = getTabButton( previousTabIndex );
        auto nextTab = getTabButton( nextTabIndex );
        /*
         If there is no previousTab, you are in the left-most position
         else If there is no nextTab, you are in the right-most position
         Otherwise you are in the middle of all the tabs.
         If you are in the middle, you might be switching with the tab on your left, or the tab on your right.
         */
        
#define DEBUG_TAB_MOVEMENTS false
#if DEBUG_TAB_MOVEMENTS
        auto getButtonName = [](auto* btn) -> juce::String
        {
            if( btn != nullptr )
                return btn->getButtonText();
            return "None";
        };
        juce::String prevName = getButtonName(previousTab);
        jassert( prevName.isNotEmpty() );
        juce::String nextName = getButtonName(nextTab);
        jassert( nextName.isNotEmpty() );
        DBG( "ETBB::itemDragMove prev: [" << prevName << "] next: [" << nextName << "]" );
#endif
        
        auto centerX = tabButtonBeingDragged->getBounds().getCentreX();
        
        if( previousTab == nullptr && nextTab != nullptr )
        {
            // you're in the 0th position (far left)
            if( centerX > nextTab->getX() )
            {
                moveTab(idx, nextTabIndex);
            }
        }
        else if( previousTab != nullptr && nextTab == nullptr )
        {
            // you're in the last position (far right)
            if( centerX < previousTab->getX() )
            {
                moveTab(idx, previousTabIndex);
            }
        }
        else
        {
            // you're in the middle
            if( centerX > nextTab->getX() )
            {
                moveTab(idx, nextTabIndex);
            }
            else if( centerX < previousTab->getRight() )
            {
                moveTab(idx, previousTabIndex);
            }
        }
        
        tabButtonBeingDragged->toFront(true);
        
    }
}

void ExtendedTabbedButtonBar::itemDragExit(const SourceDetails &dragSourceDetails)
{
    DBG("ExtendedTabbedButtonBar::itemDragExit");
    juce::DragAndDropTarget::itemDragExit(dragSourceDetails);
}

void ExtendedTabbedButtonBar::itemDropped (const SourceDetails& dragSourceDetails)
{
    DBG( "item dropped" );
    //find the dropped item. lock the position in.
    resized();
    
    auto tabs = getTabs();
    Project13AudioProcessor::DSP_Order newOrder;
    
    jassert(tabs.size() == newOrder.size());
    for(size_t i = 0; i < tabs.size(); ++i )
    {
        auto tab = tabs[ static_cast<int>(i) ];
        if( auto* etbb = dynamic_cast<ExtendedTabBarButton*>(tab) )
        {
            newOrder[i] = etbb->getOption();
        }
    }
    
    listeners.call([newOrder](Listener& l)
    {
        l.tabOrderChanged(newOrder);
    });
}

void ExtendedTabbedButtonBar::mouseDown(const juce::MouseEvent& e)
{
    DBG( "ExtendedTabbedButtonBar::mouseDown");
    if(auto tabButtonBeingDragged = dynamic_cast<ExtendedTabBarButton*>( e.originalComponent) )
    {
        startDragging(tabButtonBeingDragged->TabBarButton::getTitle(),
                      tabButtonBeingDragged,
                      dragImage);
    }
}
//==============================================================================
juce::TabBarButton* ExtendedTabbedButtonBar::createTabButton (const juce::String& tabName, int tabIndex)
{
    auto dspOption = getDSPOptionFromName(tabName);
    auto etbb = std::make_unique<ExtendedTabBarButton>(tabName, *this, dspOption);
    etbb->addMouseListener(this, false);
    
    return etbb.release();
}

void ExtendedTabbedButtonBar::addListener(Listener *l)
{
    listeners.add(l);
}

void ExtendedTabbedButtonBar::removeListener(Listener *l)
{
    listeners.remove(l);
}
//==============================================================================
DSP_Gui::DSP_Gui(Project13AudioProcessor& proc) : processor(proc)
{
    
}

void DSP_Gui::resized()
{
    auto bounds = getLocalBounds();
    if( buttons.empty() == false )
    {
        auto buttonArea = bounds.removeFromTop(30);
        auto w = buttonArea.getWidth() / buttons.size();
        for ( auto& button : buttons )
        {
            button->setBounds(buttonArea.removeFromLeft( static_cast<int>(w) ));
        }
    }
    
    if( comboBoxes.empty() == false )
    {
        auto comboBoxArea = bounds.removeFromLeft(bounds.getWidth() / 4);
        auto h = juce::jmin(comboBoxArea.getHeight() / static_cast<int>(comboBoxes.size()), 30);
        for ( auto& cb : comboBoxes )
        {
            cb->setBounds(comboBoxArea.removeFromTop( static_cast<int>(h) ));
        }
    }
    
    if( sliders.empty() == false )
    {
        auto w = bounds.getWidth() / sliders.size();
        for ( auto& slider : sliders )
        {
            slider->setBounds(bounds.removeFromLeft( static_cast<int>(w) ));
        }
    }
}
void DSP_Gui::paint( juce::Graphics& g )
{
    g.fillAll(juce::Colours::black);
}
void DSP_Gui::rebuildInterface( std::vector< juce::RangedAudioParameter* > params )
{
    sliderAttachments.clear();
    comboBoxAttachments.clear();
    buttonAttachments.clear();
    
    sliders.clear();
    comboBoxes.clear();
    buttons.clear();
    
    for( size_t i = 0; i < params.size(); ++i )
    {
        auto p = params[i];
        
        if( auto* choice = dynamic_cast<juce::AudioParameterChoice*>(p) )
        {
            //make a combobox
            comboBoxes.push_back( std::make_unique<juce::ComboBox>() );
            auto& cb = *comboBoxes.back();
            cb.addItemList(choice->choices, 1);
            comboBoxAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                                          (processor.apvts, p->getName(100), cb));
        }
        else if( auto* toggle = dynamic_cast<juce::AudioParameterBool*>(p) )
        {
            //make a toggle button
            buttons.push_back(std::make_unique<juce::ToggleButton>("Bypass"));
            auto& btn = *buttons.back();
            buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                                        (processor.apvts, p->getName(100), btn));
        }
        else
        {
            //it's a float or int param make a slider
            sliders.push_back(std::make_unique<RotarySliderWithLabels>(p, p->label, p->getName(100)));
            auto& slider = *sliders.back();
            SimpleMBComp::addLabelPairs(slider.labels, *p, p->label);
            slider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
            sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                                        (processor.apvts, p->getName(100), slider));
        }
    }
    
    for( auto& slider : sliders )
        addAndMakeVisible(slider.get());
    for( auto& cb : comboBoxes)
        addAndMakeVisible(cb.get());
    for( auto& btn : buttons )
        addAndMakeVisible(btn.get());
    
    resized();
}
//==============================================================================
Project13AudioProcessorEditor::Project13AudioProcessorEditor (Project13AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
//    dspOrderButton.onClick = [this]()
//    {
//        juce::Random r;
//        Project13AudioProcessor::DSP_Order dspOrder;
//
//        auto range = juce::Range<int>(static_cast<int>(Project13AudioProcessor::DSP_Option::Phase),
//                                      static_cast<int>(Project13AudioProcessor::DSP_Option::END_OF_LIST));
//
//        tabbedComponent.clearTabs();
//        for( auto& v : dspOrder )
//        {
//            auto entry = r.nextInt(range);
//            v = static_cast<Project13AudioProcessor::DSP_Option>(entry);
//            auto name = getDSPOptionName(v);
//            DBG( "creating tab: " << name );
//            tabbedComponent.addTab(name, juce::Colours::white, -1);
//        }
//        DBG( juce::Base64::toBase64(dspOrder.data(), dspOrder.size()));
//        jassertfalse;
        
//        audioProcessor.dspOrderFifo.push(dspOrder);
//    };
    
    //make DSP order visible to/on editor
//    addAndMakeVisible(dspOrderButton);
    //add tabbed component and make visible to editor
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(tabbedComponent);
    addAndMakeVisible(dspGUI);
    
    tabbedComponent.addListener(this);
    startTimerHz(30);
    setSize (600, 400);
}

Project13AudioProcessorEditor::~Project13AudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    tabbedComponent.removeListener(this);
}

//==============================================================================
void Project13AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void Project13AudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    //give tabbed components bounds
    
    auto bounds = getLocalBounds();
    
    /*
     within the long rectangle, instead of setting the dsp order to go from the far left of GUI to far right,
     we're going to keep it in the centre, keep the width 150px & height of 30px.
    */
    
//    dspOrderButton.setBounds(bounds.removeFromTop(30).withSizeKeepingCentre(150, 30));
    
    //adding a gap and trimming off the remaining bounds
    
    bounds.removeFromTop(10);
    tabbedComponent.setBounds(bounds.removeFromTop(30));
    dspGUI.setBounds( bounds );
}

void Project13AudioProcessorEditor::tabOrderChanged(Project13AudioProcessor::DSP_Order newOrder)
{
    rebuildInterface();
    audioProcessor.dspOrderFifo.push(newOrder);
}

void Project13AudioProcessorEditor::timerCallback()
{
    if( audioProcessor.restoreDspOrderFifo.getNumAvailableForReading() == 0 )
        return;
    
    using T = Project13AudioProcessor::DSP_Order;
    T newOrder;
    newOrder.fill(Project13AudioProcessor::DSP_Option::END_OF_LIST);
    auto empty = newOrder;
    while( audioProcessor.restoreDspOrderFifo.pull(newOrder) )
    {
        ; //do nothing -- you'll do something with the most recently pulled order next. |
    }
    
    if ( newOrder != empty ) //if you pulled nothing, neworder will be filled with END_OF_LIST
    {
        //don't create tabs if neworder is filled with END_OF_LIST
        addTabsFromDSPOrder(newOrder);
    }
}

void Project13AudioProcessorEditor::addTabsFromDSPOrder(Project13AudioProcessor::DSP_Order newOrder)
{
    tabbedComponent.clearTabs();
    for( auto v : newOrder )
    {
        tabbedComponent.addTab(getDSPOptionName(v), juce::Colours::white, -1);
    }
    
    rebuildInterface();
    //if the order is identical to the current order used by the audio side, this push will do nothing.
    audioProcessor.dspOrderFifo.push(newOrder);
}

void Project13AudioProcessorEditor::rebuildInterface()
{
    auto currentTabIndex = tabbedComponent.getCurrentTabIndex();
    auto currentTab = tabbedComponent.getTabButton(currentTabIndex);
    if( auto etbb = dynamic_cast<ExtendedTabBarButton*>(currentTab))
    {
        auto option = etbb->getOption();
        auto params = audioProcessor.getParamsForOption(option);
        jassert(params.empty() == false);
        dspGUI.rebuildInterface(params);
    }
}
        
    
    
    
    
    
    
    
