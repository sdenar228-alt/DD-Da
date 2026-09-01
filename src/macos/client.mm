#import <Cocoa/Cocoa.h>

extern "C" int TWMain(int argc, const char **argv);

// Whether we are running from inside an application bundle, in which case the
// data lives in its Resources folder and the working directory has to be moved
// there before anything looks for it.
//
// This used to compare the bundle's name against "DDNet.app". The name of the
// client is a build setting, so under any other one the comparison failed, the
// directory was never changed, and the client came up with no data at all: no
// textures, no fonts, a grey window and nothing else. What matters is being in a
// bundle, not what the bundle is called.
static BOOL IsRunningInsideAppBundle(void)
{
	NSBundle *bundle = [NSBundle mainBundle];
	if(!bundle)
		return NO;
	NSString *bundlePath = [bundle bundlePath];
	if(!bundlePath)
		return NO;
	return [bundlePath hasSuffix:@".app"];
}

int main(int argc, const char **argv)
{
	BOOL FinderLaunch = argc >= 2 && !strncmp(argv[1], "-psn", 4);

	if(IsRunningInsideAppBundle())
	{
		NSString *pResourcePath = [[NSBundle mainBundle] resourcePath];
		if(!pResourcePath)
			return -1;

		[[NSFileManager defaultManager] changeCurrentDirectoryPath:pResourcePath];
	}

	if(FinderLaunch)
	{
		const char *apArgv[2] = { argv[0], nullptr };
		return TWMain(1, apArgv);
	}
	else
		return TWMain(argc, argv);
}
