import java.io.*;

public class CharMapGen {
	private static void gen_case (boolean gcc)
	{
		System.out.println("const unsigned short cmap_toupper[65536] = {");
		for (char c = Character.MIN_VALUE; c < Character.MAX_VALUE; c ++) {
			if (Character.toUpperCase(c) != c)
				System.out.printf("[%d]=%d,\n", (int)c, (int)Character.toUpperCase(c));
		}
		System.out.println("};");

		System.out.println("const unsigned short cmap_tolower[65536] = {");
		for (char c = Character.MIN_VALUE; c < Character.MAX_VALUE; c ++) {
			if (Character.toLowerCase(c) != c)
				System.out.printf("[%d]=%d,\n", (int)c, (int)Character.toLowerCase(c));
		}
		System.out.println("};");
	}

	public static void main (String [] args) throws Exception
	{
		gen_case(true);
	}
}
