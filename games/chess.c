#include <stdio.h>
main()
{
	execl("/usr/games/lib/compat", "chess", "/usr/games/lib/chess", 0);
	execl("/usr/games/DUNGEON", "chess", "/usr/games/lib/chess", 0);
	printf(stderr, "Sorry, not today\n");
	exit(1);
}
