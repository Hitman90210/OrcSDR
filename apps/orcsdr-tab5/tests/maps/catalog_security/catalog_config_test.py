"""Exercise generated catalog configuration without ESP-IDF or private keys."""
import pathlib, subprocess, tempfile, unittest
MODULE = pathlib.Path(__file__).resolve().parents[3] / "main" / "map_catalog_config.cmake"

class CatalogConfig(unittest.TestCase):
    def run_config(self, url="", key=None):
        with tempfile.TemporaryDirectory(prefix="orcsdr-map-config-") as tmp:
            root=pathlib.Path(tmp)
            (root/"CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.20)\nproject(catalog_config NONE)\ninclude("'+MODULE.as_posix()+'")\n')
            args=["cmake","-S",str(root),"-B",str(root/"build"),"-DORCSDR_MAP_CATALOG_BASE_URL="+url]
            if key is not None:
                (root/"public.pem").write_text(key)
                args.append("-DORCSDR_MAP_CATALOG_PUBLIC_KEY_FILE="+str(root/"public.pem"))
            result=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
            header=root/"build"/"map_catalog_configuration.hpp"
            return result.returncode,header.read_text() if header.exists() else result.stdout
    def test_default_offline(self):
        code,output=self.run_config(); self.assertEqual(code,0,output)
        self.assertIn('base_url[]=R"ORCURL()ORCURL"',output)
    def test_public_source(self):
        code,output=self.run_config("https://maps.example.test/releases/v1",PUBLIC)
        self.assertEqual(code,0,output);self.assertIn(PUBLIC.strip(),output)
    def test_missing_key(self): self.assertNotEqual(self.run_config("https://maps.example.test")[0],0)
    def test_missing_url(self): self.assertNotEqual(self.run_config(key=PUBLIC)[0],0)
    def test_unsafe_urls(self):
        for url in ["http://maps.example.test", "https://user@maps.example.test", "https://maps.example.test:80", "https://maps.example.test/?token=x", 'https://maps.example.test/\"ORCURL']:
            with self.subTest(url=url):self.assertNotEqual(self.run_config(url,PUBLIC)[0],0)
    def test_private_or_oversize(self):
        for key in [PUBLIC.replace("PUBLIC", "PRIVATE"), "x"*2049, "unrecognized"]:
            with self.subTest(size=len(key)):self.assertNotEqual(self.run_config("https://maps.example.test",key)[0],0)

# Deliberately non-cryptographic fixture: this checks CMake input boundaries only.
# Signature/public-key validity belongs to the signed consumer tests.
PUBLIC="-----BEGIN PUBLIC KEY-----\n"+"A"*80+"\n-----END PUBLIC KEY-----\n"
if __name__=="__main__": unittest.main()
