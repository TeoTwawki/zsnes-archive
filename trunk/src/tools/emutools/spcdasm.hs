
import Text.ParserCombinators.Parsec hiding (spaces)
import Text.Printf

import Numeric

import Data.Maybe
import Data.List (unfoldr)
import Data.Bits
import Data.Word
import Data.Int
import Data.Char
import qualified Data.ByteString as BS
import qualified Data.IntMap as IM

import Control.Monad (liftM)

import Foreign (unsafePerformIO)
import System

-- Parser for TASM files

data Op = Op { opMnemonic :: String,
               opArgs :: String,
               opOpcode :: Int,
               opBytes :: Int,
               opRule :: String,
               opClass :: Int,
               opArgShift :: Int,
               opArgOr    :: Int                             
             } deriving (Eq, Ord, Show)


spaces = many (oneOf " \t")

pTable = do between (char '"') (char '"') (many (noneOf "\"")); newline
            xs <- many pTableEntry
            many comment
            return xs            
            
pTableEntry = do mnem <- many1 alphaNum;                  spaces
                 args <- many1 (satisfy (not . isSpace)); spaces
                 opcodeStr <- many1 hexDigit;             spaces
                 bytesStr <- many1 digit;                 spaces
                 rule <- many1 alphaNum;                  spaces
                 classStr <- many1 digit;                 spaces

                 argShift <- option 0 (liftM read (many1 digit)); spaces
                 argOr    <- option 0 (liftM (read . ("0x"++)) (many1 hexDigit))

                 newline
                 
                 return (Op { opMnemonic = mnem,
                              opArgs = args,
                              opOpcode = read ("0x"++opcodeStr),
                              opBytes = read bytesStr,
                              opRule = rule,
                              opClass = read classStr,
                              opArgShift = argShift,
                              opArgOr = argOr })
              <|> (comment >> pTableEntry)

comment = many (noneOf "\r\n") >> newline


-- Disassembler (doesn't support most TASM constructs)

optable = either (error . show) id (unsafePerformIO (parseFromFile pTable "tools/emutools/spc_asm.tab"))
opmap = IM.fromList [(opOpcode op, op) | op <- optable]

disasmOp :: BS.ByteString -> Int -> (String, Int)
disasmOp mem addr = (str, addr')
    where
    op = fromMaybe (Op { opMnemonic = "DB", opBytes = 1,
                         opArgs = "*", opRule = "T1", opClass = 1,
                         opArgShift = 0, opArgOr = 0xFF,
                         opOpcode = fromIntegral (mem `BS.index` addr) })
                   (IM.lookup (fromIntegral (mem `BS.index` addr)) opmap)

    addr' = addr + opBytes op
    str = printf "%04x: %6s %s" addr (opMnemonic op) (argStr (opArgs op) args)

    argStr "\"\""   []         = ""
    argStr ('*':ts) (arg:args) = printf "$%x" arg ++ argStr ts args
    argStr ('*':_)  []         = error ("not enough args for op " ++ show op)
    argStr (t:ts)   args       = t                :  argStr ts args
    argStr ""       []         = ""
    argStr ""       _          = error ("too many args ("++show args++") for op " ++ show op)

    args :: [Int]
    args | opBytes op == 1 =
             case opRule op of
             "NOP" -> []
             "T1" -> [(opOpcode op .&. opArgOr op) `shiftR` opArgShift op]
         | otherwise =
             map fromIntegral $
             case opRule op of
             "NOP" -> [baseArg]
             "CSWAP" -> [(baseArg `shiftR` 8) .&. 0xFF, baseArg .&. 0xFF]
             "CREL" -> [baseArg .&. 0xFF,
                        fromIntegral (addr + opBytes op + (fromIntegral (fromIntegral (baseArg `shiftR` 8) :: Int8)))]
             "R1" -> [fromIntegral (addr + opBytes op + (fromIntegral (fromIntegral baseArg :: Int8)))]
             rule -> error ("unimplemented rule "++rule)

    baseArg = realBaseArg `shiftR` opArgShift op

    realBaseArg :: Word
    realBaseArg = case opBytes op of
                  2 -> fromIntegral (BS.index mem (addr+1))
                  3 -> fromIntegral (BS.index mem (addr+1))
                       + (fromIntegral (BS.index mem (addr+2)) `shiftL` 8)
              

disasm :: BS.ByteString -> Int -> Int -> [String]
disasm mem start stop = unfoldr (\addr -> if addr < stop 
                                             then Just (disasmOp mem addr)
                                             else Nothing)
                                start

main = do [file] <- getArgs
          spcdata <- BS.readFile file
          let ram = BS.drop 0x100 spcdata
               
          putStr (unlines (disasm ram 0x0000 0x10000))
