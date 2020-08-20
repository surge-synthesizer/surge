/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "SurgePatchPlayerProcessor.h"
#include "SurgePatchPlayerEditor.h"
#include "version.h"

struct sppVUMeter : public juce::Component {
   sppVUMeter( SurgePatchPlayerAudioProcessorEditor *e ) : editor( e ) {
   }

   virtual void paint( juce::Graphics &g ) override {
      float f0 = editor->audioProcessor.vu[0];
      float f1 = editor->audioProcessor.vu[1];

      if( cf0 < threshold )
         cf0 = f0;
      target0 = f0;
      if( cf1 < threshold )
         cf1 = f1;
      target1 = f1;
      
      int w = getWidth();
      int h = getHeight();

      g.setColour( juce::Colours::black );
      g.fillRect( getLocalBounds() );
      g.setColour( juce::Colour(60,30,0) );
      g.drawRect( getLocalBounds() );

      auto grad = juce::ColourGradient::horizontal( juce::Colour( 40,40,200 ), w * 0.2, juce::Colour( 0xFF, 0x90, 0x00 ), w / 1.3 );
      auto v0 = std::min( 1.3f * cf0 * w, (float)w );
      g.setGradientFill( grad );
      g.fillRect( 1.f, 1.f, v0, h / 2.f - 2 );

      auto v1 = std::min( 1.3f * cf1 * w, (float)w );
      g.setGradientFill( grad );
      g.fillRect( 1.f, h/2.f + 1, v1, h / 2.f - 2 );

      // And now decay towards 0. The idle time is 50ms. We want 90% gone in 300ms which is 6 idles.
      // so x^6 = 0.1. .7^6 = 0.11 so good enough
      cf0 += damping * (target0 - cf0);
      cf1 += damping * (target1 - cf1);
      if( cf0 < threshold ) cf0 = 0;
      if( cf1 < threshold ) cf1 = 0;
   }

   float cf0 = 0, cf1 = 0, target0 = 0, target1 = 0, threshold=1e-4, damping = 0.75;
   SurgePatchPlayerAudioProcessorEditor* editor;
};

struct sppCategoryModel : public juce::ListBoxModel {
   sppCategoryModel( SurgePatchPlayerAudioProcessorEditor *e ) : editor( e ) {
   }

   virtual int getNumRows() override {
      return editor->audioProcessor.surge->storage.patch_category.size();
   }

   virtual void paintListBoxItem( int rowNumber, juce::Graphics &g, int w, int h, bool sel ) override {
      if( sel )
         g.setColour( juce::Colours::red );
      else
         g.setColour(juce::Colours::white);

      auto st = &(editor->audioProcessor.surge->storage);
      auto idx = st->patchCategoryOrdering[rowNumber];
      auto cat = st->patch_category[idx];
      
      g.setFont(h * 0.7f);
      g.drawText(cat.name, 5, 0, w, h, juce::Justification::centredLeft, true);
   }

   virtual void listBoxItemClicked( int rowNumber, const juce::MouseEvent &e ) override {
      auto st = &(editor->audioProcessor.surge->storage);
      auto idx = st->patchCategoryOrdering[rowNumber];
      
      editor->categorySelected( idx );
   }
   
   SurgePatchPlayerAudioProcessorEditor* editor;
};

struct sppPatchModel : public juce::ListBoxModel {
   sppPatchModel( SurgePatchPlayerAudioProcessorEditor *a ) : editor( a ) {
   }

   std::vector<int> patchIdx;
   void setCategory( int c ) {
      patchIdx.clear();
      auto st = &(editor->audioProcessor.surge->storage);
      for( auto pi : st->patchOrdering )
      {
         auto pt = st->patch_list[pi];
         if( pt.category == c )
         {
            patchIdx.push_back( pi );
         }
      }
   }
   
   virtual int getNumRows() override {
      return patchIdx.size();
   }

   virtual void paintListBoxItem( int rowNumber, juce::Graphics &g, int w, int h, bool sel ) override {
      if( sel )
         g.setColour( juce::Colours::red );
      else
         g.setColour(juce::Colours::white);
      auto st = &(editor->audioProcessor.surge->storage);
      auto pt = st->patch_list[patchIdx[rowNumber]];

      // FIXME - latoize these
      g.setFont(h * 0.7f); 
      g.drawText(pt.name, 5, 0, w, h, juce::Justification::centredLeft, true);
   }

   virtual void listBoxItemClicked( int row, const juce::MouseEvent &e ) override {
      editor->patchSelected( patchIdx[row] );
   }
   
   SurgePatchPlayerAudioProcessorEditor* editor;
};

//==============================================================================
SurgePatchPlayerAudioProcessorEditor::SurgePatchPlayerAudioProcessorEditor (SurgePatchPlayerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (700, 600);
    setResizable( true, true );
    setResizeLimits( 500, 500, 1000, 1000 );

    int boxStart = 100;
    
    categoryModel = std::make_unique<sppCategoryModel>(this);
    patchModel = std::make_unique<sppPatchModel>(this);
    patchModel->setCategory(0);
    
    categoryBox = std::make_unique<juce::ListBox>("Categories", categoryModel.get() );
    addAndMakeVisible( categoryBox.get() );
    categoryBox->updateContent();

    patchBox = std::make_unique<juce::ListBox>("Patches", patchModel.get() );

    addAndMakeVisible( patchBox.get() );
    patchBox->updateContent();

    vuMeter = std::make_unique<sppVUMeter>(this);
    addAndMakeVisible( vuMeter.get() );
    
    surgeLogo = juce::Drawable::createFromImageData (BinaryData::SurgeLogo_64_png, BinaryData::SurgeLogo_64_pngSize);
    latoTF = juce::Typeface::createSystemTypefaceFor(BinaryData::LatoRegular_ttf, BinaryData::LatoRegular_ttfSize );
    latoFont = juce::Font( latoTF );

    patchId = audioProcessor.surge->patchid;
    patchName = std::make_unique<juce::Label>("patchName", "init" );
    auto pf = latoFont;
    pf.setHeight(40);
    patchName->setFont( pf );
    patchName->setColour( juce::Label::textColourId, juce::Colours::white );
    patchName->setJustificationType( juce::Justification::centredLeft );
    addAndMakeVisible( patchName.get() );
    
    auto paf = latoFont;
    paf.setHeight(18);
    patchAuthor = std::make_unique<juce::Label>("patchAuthor", "" );
    patchAuthor->setFont( paf );
    patchAuthor->setColour( juce::Label::textColourId, juce::Colours::white );
    patchAuthor->setJustificationType( juce::Justification::centredLeft );
    addAndMakeVisible( patchAuthor.get() );

    
    resized();

    startTimer( 50 );
}

SurgePatchPlayerAudioProcessorEditor::~SurgePatchPlayerAudioProcessorEditor()
{
   stopTimer();
}

//==============================================================================
void SurgePatchPlayerAudioProcessorEditor::paint (juce::Graphics& g)
{
   // (Our component is opaque, so we must completely fill the background with a solid colour)
   // This will probaby go
   int w = getWidth();
   int h = getHeight();

   g.fillAll (juce::Colours::red );

   if( ! surgeLogo ) return;
   
   // OK so now lets first draw the logo corner
   auto r = juce::Rectangle<int>( 0, 0, logoSize + 2 * logoMargin, patchAndVU );
   g.setColour( juce::Colours::black );
   g.fillRect( r );
   surgeLogo->drawAt( g, logoMargin, logoMargin, 1.0 );

   latoFont.setHeight( 20 );
   g.setFont( latoFont );
   g.setColour( juce::Colours::white );
   g.drawText( "Player", logoMargin, logoSize, logoSize, patchAndVU - logoSize - logoMargin, juce::Justification::centredTop );

   auto cg = juce::ColourGradient::horizontal( juce::Colours::black, logoSize + 2 * logoMargin, juce::Colour( 60, 30, 0 ), w - logoSize - 2 * logoMargin );
   g.setGradientFill(cg);
   g.fillRect( logoSize + 2 * logoMargin, 0,  w - logoSize - 2 * logoMargin, patchAndVU );


   // Now draw the next two sections and then the dividing lines
   g.setColour( juce::Colour( 60, 30, 0 ) );
   g.fillRect( 0, patchAndVU, w, controlsAndMacros );
   g.setColour( juce::Colours::white );
   g.drawText( "Soon: MPE, Tuning, PolyCount, Macros etc", logoMargin, patchAndVU + logoMargin, w - 2 * logoMargin, 50, juce::Justification::centredTop );

   g.setColour( juce::Colour( 0, 0, 60 ) );
   g.fillRect( 0, patchAndVU + controlsAndMacros, w, h - patchAndVU - controlsAndMacros );


   // Draw the words "Category" and "Patch"
   latoFont.setHeight( 20 );
   g.setFont( latoFont );
   g.setColour( juce::Colours::white );
   int cAndPStart = patchAndVU + controlsAndMacros;
   g.drawText( "Category", 0, cAndPStart + 4, getWidth()/2, 24, juce::Justification::centred);
   g.drawText( "Patch", getWidth()/2, cAndPStart + 4, getWidth()/2, 24, juce::Justification::centred );


   // Then finally the about info
   latoFont.setHeight( 12 );
   g.setFont( latoFont );
   g.setColour( juce::Colours::white );
   std::string vi = std::string( "version: " ) + Surge::Build::GitBranch + " / " + Surge::Build::GitHash + " / " + Surge::Build::BuildDate;
   g.drawText( vi.c_str(), logoMargin, h - aboutInfo, w - logoMargin, aboutInfo, juce::Justification::centredLeft );
   
   g.setColour( juce::Colour( 0xFF, 0x90, 0x00 ) );
   g.fillRect( 0, patchAndVU, w, 2 );
   g.fillRect( 0, patchAndVU + controlsAndMacros, w, 2 );
}

void SurgePatchPlayerAudioProcessorEditor::resized()
{
   int w = getWidth();
   int h = getHeight();

   if( ! categoryBox.get() ) return;

   patchName->setBounds( logoSize + 2 * logoMargin, logoMargin, w - logoMargin - 3 * logoMargin, 40 );
   patchAuthor->setBounds( logoSize + 2 * logoMargin, logoMargin + 40, w - logoMargin - 3 * logoMargin, 18 );
   
   int vuHeight = 20;
   vuMeter->setBounds( logoSize + 2 * logoMargin, patchAndVU - vuHeight - logoMargin, w - logoSize - 3 * logoMargin, vuHeight );
   
   int cAndPStart = patchAndVU + controlsAndMacros;
   int cAndPHeight = h - cAndPStart - aboutInfo;

   int roomForText = 25;
   categoryBox->setBounds( 4, cAndPStart + 4 + roomForText, getWidth()/2 - 8, cAndPHeight - 8 - roomForText );
   patchBox->setBounds( getWidth()/2 + 4, cAndPStart + 4 + roomForText, getWidth()/2 - 8, cAndPHeight - 8 - roomForText);
}

void SurgePatchPlayerAudioProcessorEditor::categorySelected( int row )
{
   patchModel->setCategory(row);
   patchBox->updateContent();
}

void SurgePatchPlayerAudioProcessorEditor::patchSelected( int patchidx )
{
   // I have to think hard about asynchronicity here. We probalby want to do the patch queue thing and then notify back on quueu change
   // to repaint components
   audioProcessor.surge->loadPatch( patchidx );
}

void SurgePatchPlayerAudioProcessorEditor::idle()
{
   if( vuMeter ) vuMeter->repaint();
   
   if( patchId != audioProcessor.surge->patchid )
   {
      patchId = audioProcessor.surge->patchid;
      patchName->setText( audioProcessor.surge->storage.getPatch().name, juce::dontSendNotification );
      patchAuthor->setText( std::string( "Author: " ) + audioProcessor.surge->storage.getPatch().author, juce::dontSendNotification );
   }
}
