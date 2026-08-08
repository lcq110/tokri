#include "MacQuickLook.h"

#include <QByteArray>

#import <QuickLookUI/QuickLookUI.h>

@interface TokriQuickLookController : NSObject <QLPreviewPanelDataSource, QLPreviewPanelDelegate>
@property(nonatomic, retain) NSURL *previewURL;
@end

@implementation TokriQuickLookController

- (NSInteger)numberOfPreviewItemsInPreviewPanel:(QLPreviewPanel *)panel
{
    return self.previewURL ? 1 : 0;
}

- (id<QLPreviewItem>)previewPanel:(QLPreviewPanel *)panel
               previewItemAtIndex:(NSInteger)index
{
    return self.previewURL;
}

- (BOOL)previewPanel:(QLPreviewPanel *)panel handleEvent:(NSEvent *)event
{
    if (event.type == NSEventTypeKeyDown
        && [event.charactersIgnoringModifiers isEqualToString:@" "]) {
        [panel orderOut:nil];
        return YES;
    }

    return NO;
}

@end

void MacQuickLook::toggle(const QString &filePath)
{
    @autoreleasepool {
        QLPreviewPanel *panel = [QLPreviewPanel sharedPreviewPanel];
        if (panel.visible) {
            [panel orderOut:nil];
            return;
        }

        static TokriQuickLookController *controller =
            [[TokriQuickLookController alloc] init];
        const QByteArray path = filePath.toUtf8();
        controller.previewURL = [NSURL fileURLWithPath:
            [NSString stringWithUTF8String:path.constData()]];

        panel.dataSource = controller;
        panel.delegate = controller;
        panel.currentPreviewItemIndex = 0;
        [panel reloadData];
        [panel makeKeyAndOrderFront:nil];
    }
}
